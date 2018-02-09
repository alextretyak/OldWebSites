#pragma once


//Timer
/*DWORD _timerStart;

inline void StartTimer()
{
	SYSTEMTIME t;
	GetSystemTime(&t);
	_timerStart=t.wMilliseconds;
}

float GetTimer()
{
	SYSTEMTIME t;
	GetSystemTime(&t);
	return (float)(t.wMilliseconds-_timerStart)/1000.0f;
}*/

//КЛАССЫ КОНТЕЙНЕРЫ

///////////////////////////////////////////АЛЛОКАТОРЫ//////////////////////////////////////////
template <class T> class std_allocator
{
public:
	inline T *alloc() {return new T;}
	inline void free(T *p) {delete p;}
	inline operator bool () {return true;}
};

template <class T> class stack_allocator
{
	dword size,top;
	T **st;//в стеке - свободные ячейки

	byte *block;//У каждого блока: 4b - указатель на след. блок (или NULL), далее - данные типа T

public:
	inline stack_allocator():st(0),size(0),top(0),block(0) {}
	inline stack_allocator(dword size):size(size)
	{
		st=new T* [size];
		block=new byte [sizeof(byte*)+size*sizeof(T)];
		*(byte**)block=0;

		T *t=(T*)(block+sizeof(byte*));
		for (top=0;top<size;top++,t++)
			st[top]=t;
	}
	inline ~stack_allocator() {destroy();}

	inline void operator() (dword size)
	{
		destroy();

		stack_allocator::size=size;
		st=new T* [size];
		block=new byte [sizeof(byte*)+size*sizeof(T)];
		*(byte**)block=0;

		T *t=(T*)(block+sizeof(byte*));
		for (top=0;top<size;top++,t++)
			st[top]=t;
	}

	inline void destroy()
	{
		size=top=0;
		if (st) {delete [] st; st=0;}
		if (block)
		{
			while (block)
			{
				byte *n=*(byte**)block;
				delete [] block;
				block=n;
			}

			block=0;
		}
	}

	inline T *alloc()
	{
		if (!top)//стек пуст - свободных элементов нет
		{
			delete [] st;

			byte *n=block;
			block=new byte [sizeof(byte*)+size*sizeof(T)];
			*(byte**)block=n;

			st=new T* [size*2];
			T *t=(T*)(block+sizeof(byte*));
			for (;top<size;top++,t++)
				st[top]=t;

			size*=2;
		}

		top--;
		return st[top];
	}

	inline void free(T *p)
	{
		st[top++]=p;
	}

	inline operator bool () {return size!=0;}
};

template <class T> class binary_allocator
{
	dword max_size,nADs,max_ADs;

	class allocator_data
	{
	public:

		bool *free;//таблица свободных блоков
		T *data;

		//allocator_data() {}
		allocator_data(dword size)
		{
			data=new T [size];
			size=2*size-1;
			free=new bool [size];
			free_all(size);
		}
		~allocator_data()
		{
			delete [] free;
			delete [] data;
		}

		inline void free_all(dword size)
		{
			//for (register dword i=0;i<size;i++)
			//	free[i]=true;
			memset(free,true,size);
		}

		inline T *alloc(dword size)
		{
			size--;
			for (register dword n=1;n<size;)
				n=free[n]?(2*n+1):(2*n+3);

			if (!free[n]) n++;
			free[n]=false;
			T *res=&data[n-size];

			for (n=(n-1)/2;n>0;n=(n-1)/2)
				free[n]=free[2*n+1] || free[2*n+2];
			free[0]=free[1] || free[2];
			return res;
		}

		inline void free_el(T *P,dword size)
		{
			for (register dword n=P-data+size-1;n>0;n=(n-1)/2)
				free[n]=true;
			free[0]=true;
		}
	} **AD,*lastFreeAD;//lastFreeAD - последний свободный блок

public:

//	allocator() {AD=0;}
	binary_allocator(dword sz,dword _nADs=10)
	{
		__asm
		{
			mov edx,1
			mov eax,sz
			cmp eax,0
			cmove eax,edx//if (sz==0) sz=1;
			dec eax
			bsr ecx,eax
			inc ecx
			shl edx,cl//edx=max_size
			mov sz,edx
		}
		AD=new allocator_data* [max_ADs=_nADs];
		nADs=1;
		AD[0]=new allocator_data(max_size=sz);
		lastFreeAD=AD[0];
	}
	~binary_allocator() {destroy();}

	inline void destroy()
	{
		if (AD)
		{
			for (register dword i=0;i<nADs;i++)
				delete AD[i];
			delete [] AD;
			AD=0;
		}
	}

	inline void operator() (dword sz,dword _nADs=10)
	{
		destroy();
		__asm
		{
			mov edx,1
			mov eax,sz
			cmp eax,0
			cmove eax,edx//if (sz==0) sz=1;
			dec eax
			bsr ecx,eax
			inc ecx
			shl edx,cl//edx=max_size
			mov sz,edx
		}
		AD=new allocator_data* [max_ADs=_nADs];
		nADs=1;
		AD[0]=new allocator_data(max_size=sz);
		lastFreeAD=AD[0];
	}

//	operator dword () {return size;}
	inline operator bool () {return AD!=0;}

	inline void free_all()
	{
		dword size=2*max_size-1;
		for (register dword i=0;i<nADs;i++)
			AD[i]->free_all(size);
	}

	inline dword find_pointer(T *P)//Возвращает номер аллокатора в котором нах-ся указатель
	{
		dword min=0,max=nADs,mid;
		while (min!=max-1)
		{
			mid=(max+min)/2;
			if (P<AD[mid]->data)
				max=mid;
			else if (P>=(AD[mid]->data+max_size))
				min=mid;
			else return mid;
		}
		return min;
	}

	inline T *alloc()
	{
		if (lastFreeAD->free[0])
			return lastFreeAD->alloc(max_size);
		else//Блок заполнен
		{
			for (register dword i=0;i<nADs;i++)//Ищем свободный блок
				if (AD[i]->free[0]) {lastFreeAD=AD[i]; return AD[i]->alloc(max_size);}

			//Все блоки заняты - создаём новый блок
			if (nADs<max_ADs)
			{
				allocator_data *tAD=new allocator_data(max_size);
				dword n=(tAD->data<AD[0]->data)?0:(find_pointer(tAD->data)+1);
				for (register dword i=nADs++;i>n;i--)
					AD[i]=AD[i-1];
				AD[n]=tAD;
				return (lastFreeAD=tAD)->alloc(max_size);
			}
			else//создаём новый массив блоков
			{
				allocator_data **newAD=new allocator_data* [max_ADs*=2];
				allocator_data *tAD=new allocator_data(max_size);

				dword n=(tAD->data<AD[0]->data)?0:(find_pointer(tAD->data)+1);
				for (i=nADs++;i>n;i--)
					newAD[i]=AD[i-1];
				newAD[n]=tAD;
				for (i=0;i<n;i++)
					newAD[i]=AD[i];
				delete [] AD;
				AD=newAD;
				return (lastFreeAD=tAD)->alloc(max_size);
			}
		}
	}

	inline void free(T *P)
	{
		AD[find_pointer(P)]->free_el(P,max_size);
	}
};
/////////////////////////////////////////////////////////////////////////////////////////////////////

template <class T_DATA> struct slist_node
{
	slist_node *next;
	T_DATA value;

//	node() {next=0;}
};

template < class T_DATA, class T_ARG=T_DATA&, class allocator=std_allocator< slist_node<T_DATA> > > class slist//singly linked list
{
	allocator *A;

	/*void clear_node(node *next)
	{
		if (next->next) clear_node(next->next);
		A->free(next);
	}*/

public:

	typedef slist_node<T_DATA> node;

	dword size;
	node *head,*tail;

	class iterator
	{
	public:
		node *value;

		inline iterator() {}
		inline iterator(node *v):value(v) {}
		inline iterator(slist &l):value(l.head) {}

		inline void operator= (node *v) {value=v;}
		inline void operator= (slist &l) {value=l.head;}
		//operator bool () {return value;}
		inline operator node* () {return value;}
		inline void operator++ (int notused) {value=value->next;}
		inline T_DATA &operator* () {return value->value;}
		inline T_DATA *operator-> () {return &(value->value);}
		inline T_DATA *operator& () {return &(value->value);}
	};

	class iterator2
	{
	public:
		node **p;

		inline iterator2() {}
		inline iterator2(slist &l):p(&l.head) {}

		inline void operator= (slist &l) {p=&l.head;}
		inline operator node* () {return *p;}
		inline void operator++ (int notused) {p=&(**p).next;}
		inline T_DATA &operator* () {return (**p).value;}
		inline T_DATA *operator-> () {return &((**p).value);}
		inline T_DATA *operator& () {return &((**p).value);}

		inline operator iterator() {return *p;}
	};

	inline slist():A(0),head(0),tail(0),size(0) {}
	inline slist(allocator &_A):A(&_A),head(0),tail(0),size(0) {}
	inline ~slist() {clear();}

	inline void operator() (allocator &_A) {A=&_A;}

	inline void clear()
	{
		if (head && (A?*A:true))
		{
			//clear_node(head);
			while (head)
			{
				node *n=head->next;
				A->free(head);
				head=n;
			}
			tail=0;
			size=0;
		}
	}

	inline void push_head(T_ARG val)//Добавляет элемент в начало списка
	{
		node *n=head;
		head=A->alloc();
		head->next=n;
		head->value=val;
		if (!tail) tail=head;
		size++;
	}

	inline void pop_head()//Извлекает элемент из начала списка
	{
		if (head)
		{
			node *n=head->next;
			A->free(head);
			head=n;
			if (!head) tail=0;
			size--;
		}
	}

	inline void push_tail(T_ARG val)//Добавляет элемент в конец списка
	{
		if (tail)
		{
			tail->next=A->alloc();
			tail->next->next=0;
			tail->next->value=val;
			tail=tail->next;
		}
		else
		{
			tail=head=A->alloc();
			tail->next=0;
			tail->value=val;
		}
		size++;
	}

	inline void pop_tail()//Извлекает элемент из конца списка// !!! O(n) !!!
	{
		if (tail)
		{
			if (head->next)
			{
				tail=head;
				while (tail->next->next) tail=tail->next;
				A->free(tail->next);
				tail->next=0;
			}
			else
			{
				A->free(head);
				head=tail=0;
			}
			size--;
		}
	}

	inline T_DATA &get_head()
	{
		return head->value;
	}

	inline T_DATA &get_tail()
	{
		return tail->value;
	}

	inline bool find(T_ARG key,iterator &it)
	{
		iterator i=*this;

		while (i)
		{
			if (*i==key)
			{
				it=i;
				return true;
			}
			i++;
		}

		return false;
	}

	inline bool find(T_ARG key,iterator2 &it)
	{
		iterator2 i=*this;

		while (i)
		{
			if (*i==key)
			{
				it=i;
				return true;
			}
			i++;
		}

		return false;
	}

	inline void insert(iterator i,T_ARG val)//!!! O(n) !!!
	{
		if (head)
		{
			if (i.value==head)
			{
				head=A->alloc();
				head->next=i;
				head->value=val;
			}
			else
			{
				node *prev=head;
				do
				{
					if (i.value==prev->next)
					{
						prev->next=A->alloc();
						prev->next->next=i;
						prev->next->value=val;
						break;
					}

					prev=prev->next;
				} while (prev);
			}
			size++;
		}
	}

	inline void insert(iterator2 i,T_ARG val)
	{
		node *n=*(i.p);
		*(i.p)=A->alloc();
		(*(i.p))->next=n;
		(*(i.p))->value=val;
		size++;
	}

	inline void del(iterator i)//!!! O(n) !!!
	{
		if (head)
		{
			if (i.value==head)
			{
				node *n=head->next;
				A->free(head);
				head=n;
				if (!head) tail=0;
			}
			else
			{
				node *prev=head;
				do
				{
					if (i.value==prev->next)
					{
						node *n=prev->next->next;
						A->free(prev->next);
						prev->next=n;
						if (!n) tail=prev;
						break;
					}

					prev=prev->next;
				} while (prev);
			}
			size--;
		}
	}

	inline void del(iterator2 i)
	{
		if (*(i.p)==tail) if (tail!=head) tail=(node*)i.p; else tail=0;

		node *n=(*(i.p))->next;
		A->free(*(i.p));
		*(i.p)=n;

		size--;
	}

//	operator bool () {return head;}
	inline bool empty() {return head==0;}
	inline operator node* () {return head;}
	inline T_DATA &operator* () {return head->value;}
	inline T_DATA *operator-> () {return &(head->value);}
};

#define slist_stack_allocator(T) stack_allocator <slist_node<T> >
#define slist_binary_allocator(T) binary_allocator <slist_node<T> >

template <class T_DATA> struct binary_tree_node
{
	binary_tree_node *left,*right;
	T_DATA value;
};

template < class T_DATA, class T_ARG=T_DATA&, class allocator=std_allocator< binary_tree_node<T_DATA> > > class binary_tree
{
	typedef binary_tree_node<T_DATA> node;

private:

	allocator *A;

	void clear_node(node *nd)
	{
		if (nd->left) clear_node(nd->left);
		if (nd->right) clear_node(nd->right);
		A->free(nd);
	}

	T_DATA searching_value;
	node **find_in_node(node **nd)
	{
		if (searching_value==(*nd)->value) return nd;

		node **res;
		if ((*nd)->left) if (res=find_in_node(&(*nd)->left)) return res;
		if ((*nd)->right) if (res=find_in_node(&(*nd)->right)) return res;

		return 0;
	}

public:

	node *root;

	inline binary_tree() {A=0; root=0;}
	inline binary_tree(allocator &_A) {A=&_A; root=0;}
	inline ~binary_tree() {clear();}

	inline void clear() {if (root && (A?*A:true)) {clear_node(root); root=0;}}

	inline void insert(T_ARG value)
	{
		if (root)
		{
			node *nd=root;

			while (true)
			{
				if (value<nd->value)//Вставляем в левую ветвь
				{
					if (nd->left)
						nd=nd->left;
					else
					{
						nd->left=A->alloc();
						nd->left->left=nd->left->right=0;
						nd->left->value=value;
						return;
					}
				}
				else//Вставляем в правую ветвь
				{
					if (nd->right)
						nd=nd->right;
					else
					{
						nd->right=A->alloc();
						nd->right->left=nd->right->right=0;
						nd->right->value=value;
						return;
					}
				}
			}
		}
		else
		{
			root=A->alloc();
			root->left=root->right=0;
			root->value=value;
		}
	}

	void get_left(T_DATA &val)
	{
		if (root)
		{
			node *t=root;

			while (t->left) t=t->left;

			val=t->value;
		}
	}

	void pop_left(T_DATA &val)
	{
		if (root)
		{
			if (root->left)
			{
				node *t=root;
				while (t->left->left) t=t->left;

				val=t->left->value;
				A->free(t->left);
				t->left=t->left->right;
			}
			else
			{
				val=root->value;
				A->free(root);
				root=root->right;
			}
		}
	}

	node **find_in_whole(T_ARG t)
	{
		if (root)
		{
			searching_value=t;
			return find_in_node(&root);
		}
		else return 0;
	}

	void del(node **pos)
	{
		if (pos)
		{
			if (*pos)
			{
				node *i=(**pos).right;
				while (i->left) i=i->left;
				i->left=(**pos).left;
				A->free(*pos);
				*pos=(**pos).right;
			}
		}
	}

	inline operator node* () {return root;}
};

#define binary_tree_stack_allocator(T) stack_allocator <binary_tree_node<T> >
#define binary_tree_binary_allocator(T) binary_allocator <binary_tree_node<T> >
//#define binary_tree_binary_allocator_arg(T,T_ARG) binary_allocator <binary_tree<T,T_ARG>::node>

template <class T> class storage
{
	dword max_size;
	class node
	{
	public:

		dword size;
		T *data;
		node *next;

		node(dword max_sz,dword sz=0)
		{
			size=sz;
			data=new T [max_sz];
		}
		~node() {delete [] data;if (next) delete next;}
	} *head;
public:
	storage():head(0) {}
	storage(dword sz)
	{
		if (sz==0) sz=1;
		head=new node(max_size=sz);
		head->next=0;
	}
	~storage() {destroy();}

	void destroy() {if (head) {delete head; head=0;}}

	void operator() (dword sz)
	{
		destroy();
		if (sz==0) sz=1;
		head=new node(max_size=sz);
		head->next=0;
	}

	T *alloc(dword n=1)
	{
		if (n>max_size) {iWarning("Размер массива больше размера блока"); return &head->data[0];}

		if (head->size+n<=max_size)//В текущем блоке ещё есть место
		{
			T *res=&head->data[head->size];
			head->size+=n;
			return res;
		}
		else//Создаем новый блок
		{
			node *t=new node(max_size,n);
			t->next=head;
			head=t;
			return head->data;
		}
	}
};

template <class T_DATA,class T_ARG=T_DATA&> class array//динамический массив
{
	dword max_size;
public:
	T_DATA *data;
	dword size;

	array():data(0) { /*max_size=0;*/}// size=0;}
	array(dword sz)
	{
		if (sz==0) sz=1;
		size=0;
		data=new T_DATA [max_size=sz];
	}
	~array() {destroy();}

	void destroy() {if (data) {delete [] data; data=0;}}// size=0;}

	void operator()(dword sz)
	{
		destroy();
		if (sz==0) sz=1;
		size=0;
		data=new T_DATA [max_size=sz];
	}

	void operator= (const array &a)
	{
		size=a.size;
		if (max_size<a.size)//нужен новый массив
		{
			/*if (data) */delete [] data;
			data=new T_DATA [max_size=a.max_size];
		}
		//max_size=a.max_size;
		for (register dword i=0;i<size;i++)
			data[i]=a.data[i];
	}

	inline bool empty() {return size==0;}

	inline void clear() {size=0;}

	void add(T_ARG t)
	{
		if (size==max_size)//массив переполнен
		{
			T_DATA *new_data=new T_DATA [max_size*=2];
			//for (register dword i=0;i<size;i++)
			//	new_data[i]=data[i];
			memcpy(new_data,data,size*sizeof(T_DATA));
			delete [] data;
			data=new_data;
		}

		data[size++]=t;
	}

	T_DATA* add()//возвращает последний элемент
	{
		if (size==max_size)//массив переполнен - выделяем место для нового элемента
		{
			T_DATA *new_data=new T_DATA [max_size*=2];
			//for (register dword i=0;i<size;i++)
			//	new_data[i]=data[i];
			memcpy(new_data,data,size*sizeof(T_DATA));
			delete [] data;
			data=new_data;
		}

		return &data[size++];
	}

	//void add() {size++;}

	inline void del(dword n)
	{
		if ((--size)!=n)//элемент не последний
//		if (--size)//элемент не последний
			data[n]=data[size];
//		data[n]=data[--size];
	}

	void del_shift(dword n)
	{
		size--;
		for (register dword i=n;i<size;i++)
			data[i]=data[i+1];
	}

	void insert(dword n,T_ARG t)//вставка
	{
		if (size==max_size)//массив переполнен
		{
			T_DATA *new_data=new T_DATA [max_size*=2];
			for (register dword i=size++;i>n;i--)
				new_data[i]=data[i-1];
			new_data[n]=t;
			for (i=0;i<n;i++)
				new_data[i]=data[i];
			delete [] data;
			data=new_data;
		}
		else
		{
			for (register dword i=size++;i>n;i--)
				data[i]=data[i-1];
			data[n]=t;
		}
	}

	int find(T_ARG t)
	{
		for (register dword i=0;i<size;i++)
			if (data[i]==t) return i;

		return -1;
	}

//#define AAAAAAAAAAAAAAAAAAAA
#ifdef AAAAAAAAAAAAAAAAAAAA
	inline T_DATA &operator[] (dword n)
	{
	//	if (n>=size) throw "gh";//{ERR; return data[0];}
		return data[n];
	}
#else
//	inline operator dword () {return size;}//размер массива
	inline operator T_DATA* () {return data;}//данные
#endif
};

template <class T_DATA,class T_ARG=T_DATA&> class stack
{
	dword max_size;
public:
	T_DATA *data;
	dword top;

	inline stack() {data=0;}
	inline stack(dword sz)
	{
		if (sz==0) sz=1;
		top=0;
		data=new T_DATA [max_size=sz];
	}
	inline ~stack() {destroy();}

	inline void destroy()
	{if (data) {delete [] data; data=0;}}

	inline void operator()(dword sz)
	{
		if (sz==0) sz=1;
		top=0;
		data=new T_DATA [max_size=sz];
	}

	inline void clear() {top=0;}

	inline bool empty() {return top==0;}

	inline void push(T_ARG el)
	{
		if (top==max_size)
		{
			T_DATA *new_data=new T_DATA [max_size*=2];
			//for (register dword i=0;i<top;i++)
			//	new_data[i]=data[i];
			memcpy(new_data,data,top*sizeof(T_DATA));
			delete [] data;
			data=new_data;
		}

		data[top++]=el;
	}

	inline T_DATA &pop()
	{
		if (top)
		{
			top--;
			return data[top];
		}

		return data[0];
	}

	inline operator T_DATA* () {return &data[top-1];}//данные
};

template <class T> class buffer
{
	void alloc(dword _sz)
	{
		if (!data)
		{
			data=new T [size=_sz];
			p=data;
		}
		else if (_sz>size)
		{
			delete [] data;
			data=new T [size=_sz];
			p=data;
		}
	}
public:

	T *data,*p;
	dword size;

	buffer():data(0) {}
	buffer(dword _sz) {alloc(_sz);}
	~buffer() {destroy();}

	operator T* () {return data;}

	T *operator () (dword _sz) {alloc(_sz); return data;}

	void destroy()
	{
		if (data) {delete [] data; data=0;}
	}

	void *add(dword _sz)
	{
		void *res=p;
		p+=_sz;
		return res;
	}
};

template <dword pages,class ID_TYPE,class DATA_TYPE> class cash
{
	DATA_TYPE data[pages];//DATA_TYPE should have clear() and fill(ID_TYPE id)
	dword assoc_count;
	struct assoc_t
	{
		dword n;
		ID_TYPE id;
	} assoc[pages];

public:

	cash():assoc_count(0) {}

	void clear()
	{
		for (dword i=0;i<assoc_count;i++)
			data[i].clear();

		assoc_count=0;
	}

	DATA_TYPE *operator[] (ID_TYPE id)
	{
		register dword i;

		for (i=0;i<assoc_count;i++)
			if (assoc[i].id==id)
			{
				assoc_t a=assoc[i];
				//del_shift(i)
				for (i++;i<assoc_count;i++)
					assoc[i-1]=assoc[i];
				//add(a)
				assoc[assoc_count-1]=a;
				return &data[a.n];
			}
iWarning("Cash missing");
		if (assoc_count<pages)
		{
			assoc_t a;
			data[a.n=assoc_count].fill(a.id=id);
			assoc[assoc_count++]=a;
			return &data[a.n];
		}

		assoc_t a={assoc[0].n,id};
		//del_shift(0)
		for (i=0;i<pages-1;i++)
			assoc[i]=assoc[i+1];

		//data[a.n].clear(id);
		data[a.n].fill(id);
		assoc[a.n]=a;
		return &data[a.n];
	}
};

template <class T,class KeyT> int iBinarySearch(int count,T *item,KeyT key)
{
	int low=0, high=count-1, mid;

	while (low<=high)
	{
		mid=(low+high)/2;
		if (key<item[mid]) high=mid-1;
		else if (key>item[mid]) low=mid+1;
		else return mid;
	}

	return -1-low;
}

template <class T> void ___qs(T *item,int left,int right)
{
	register int i,j;
	T x,y;

	i=left; j=right;
	x=item[(left+right)/2];

	do
	{
		while (item[i]<x && i<right) i++;
		while (x<item[j] && j>left) j--;

		if (i<=j)
		{
			y=item[i];
			item[i]=item[j];
			item[j]=y;
			i++;
			j--;
		}
	} while (i<=j);

	if (left<j) ___qs(item,left,j);
	if (i<right) ___qs(item,i,right);
}

template <class T> void iQuickSort(T *item,int count)
{
	___qs(item,0,count-1);
}
