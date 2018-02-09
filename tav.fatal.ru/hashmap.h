inline unsigned Hash(const string &s)
{
	unsigned hash = 0;
	for (size_t i=0,l=s.length()%sizeof(unsigned),e=s.length()-l; i<l; i++)
		hash |= unsigned(s[e+i]) << (i*8);

	for (size_t i=0; i<s.length()/sizeof(unsigned); i++)
		hash ^= ((unsigned*)s.data())[i];

	return hash;
}

template <unsigned Size, class ValueT, class KeyT=string> class FixedHashMap
{
	struct HashElement
	{
		bool used;
		KeyT key;
		ValueT value;

		HashElement():used(false) {}
	};

	HashElement elements[Size];

public:

	ValueT *next(unsigned &i)
	{
		for (; i<Size; i++)
			if (elements[i].used)
			{
				i++;
				return &elements[i].value;
			}

		return NULL;
	}

	ValueT *add(const KeyT &key)
	{
		unsigned n = Hash(key) % Size;
		if (!elements[n].used)
		{
			elements[n].used = true;
			elements[n].key = key;
			return &elements[n].value;
		}
		return NULL;
	}

	bool add(const KeyT &key,const ValueT &value)
	{
		unsigned n = Hash(key) % Size;
		if (!elements[n].used)
		{
			elements[n].used = true;
			elements[n].key = key;
			elements[n].value = value;
			return true;
		}
		return false;
	}

	ValueT *operator[](const KeyT &key)
	{
		unsigned n = Hash(key) % Size;
		if (elements[n].used && elements[n].key==key)
			return &elements[n].value;

		return NULL;
	}

	const ValueT *operator[](const KeyT &key) const
	{
		unsigned n = Hash(key) % Size;
		if (elements[n].used && elements[n].key==key)
			return &elements[n].value;

		return NULL;
	}
};


template <unsigned BlockSize, class ItemT> class ItemsStorage //простое хранилище элементов одинаковой длины
{
	struct Block //выделение места под элементы идёт блоками фиксированной длины
	{
		ItemT data[BlockSize];
		Block *next;
	} *first;

	unsigned used;

public:

	ItemsStorage()
	{
		first = new Block;
		first->next = NULL;
		used = 0;
	}

	~ItemsStorage()
	{
		for (Block *next; first; first = next)
		{
			next = first->next;
			delete first;
		}
	}

	ItemT *alloc()
	{
		if (used < BlockSize) return &first->data[used++];

		Block *newBlock = new Block;
		newBlock->next = first;
		first = newBlock;
		used = 1;
		return first->data;
	}
};

template <unsigned BlockSize, class KeyT, class ValueT> class StorageHashMap //расширяемая хэш-таблица, без удаления элементов
{
	struct ListItem
	{
		KeyT key;
		ValueT value;

		ListItem *next;
	};

	ItemsStorage<BlockSize, ListItem> itemsStorage;
	ListItem **table;
	unsigned tableSize,used;

	friend class Iterator;

public:

	struct Iterator
	{
		StorageHashMap *hashMap;
		unsigned tableIndex;
		ListItem *item;

		ListItem &operator*() { return *item; }
		ListItem *operator->() { return item; }

		Iterator &operator++()
		{
			if (item) item = item->next;

			while (item == NULL)
			{
				if (++tableIndex < hashMap->tableSize)
					item = hashMap->table[ tableIndex ];
				else
				{
					item = NULL;
					break;
				}
			}

			return *this;
		}

		Iterator operator++(int)
		{
			Iterator p = *this;
			++*this;
			return p;
		}

		bool operator==(const Iterator &i) const
		{
			return hashMap == i.hashMap && tableIndex == i.tableIndex && item == i.item;
		}

		bool operator!=(const Iterator &i) const { return !operator==(i); }
	};

	Iterator begin()
	{
		Iterator it = {this, (unsigned)-1, NULL};
		++it;
		return it;
	}

	Iterator end()
	{
		Iterator it = {this, tableSize, NULL};
		return it;
	}

	StorageHashMap():table(NULL),tableSize(0),used(0) {}

	~StorageHashMap()
	{
		if (table) delete table;
	}

	void reserve(unsigned size)
	{
		ListItem **newTable = new ListItem* [size];
		memset(newTable, 0, sizeof(ListItem*)*size);

		if (table) //переносим все данные из старой таблицы в новую
		{
			for (unsigned i=0; i<tableSize; i++)
				for (ListItem *sli = table[i], *next; sli; sli = next)
				{
					next = sli->next;
					ListItem **li = newTable + Hash(sli->key) % size;
					sli->next = *li;
					*li = sli;
				}

			delete table;
		}

		table = newTable;
		tableSize = size;
	}

	unsigned size() const { return used; }

	ValueT &add(const KeyT &key)
	{
		if (used >= tableSize*3/4) //занято уже 75% таблицы или более
			reserve( max(1u, tableSize * 2) );

		ListItem **li = table + Hash(key) % tableSize;
		while (*li)
		{
			if ((*li)->key == key) return (*li)->value; //такой элемент уже есть
			li=&(*li)->next;
		}

		//Добавляем новый элемент
		used++;
		ListItem *newLI = itemsStorage.alloc();
		*li = newLI;
		newLI->key  = key;
		newLI->next = NULL;
		return newLI->value;
	}

	ValueT *operator[](const KeyT &key)
	{
		unsigned n = Hash(key) % tableSize;

		for (ListItem *li = table[n]; li; li = li->next)
			if (li->key == key) return &li->value;

		return NULL;
	}

	const ValueT *operator[](const KeyT &key) const
	{
		unsigned n = Hash(key) % tableSize;

		for (ListItem *li = table[n]; li; li = li->next)
			if (li->key == key) return &li->value;

		return NULL;
	}
};
