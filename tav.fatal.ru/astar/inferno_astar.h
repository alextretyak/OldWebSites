#include "inferno_internal.h"

template <class dataT> class Astar
{
public:
	class Astar_node
	{
	public:

		dataT data;
		/*dataT should have following methods:
			float cost(dataT &);
			//void neighbours(Astar <dataT>::Astar_node &n,Astar <dataT> *A); //Should call A->neighbour(n,n') for each sucessor n'
			bool operator==(dataT &d);
			bool operator<(dataT &d);
			bool operator>(dataT &d);
			bool is_goal();
			float goal_dist_estimate(); //Approximately distance to goal
		*/
		float f,g,h;
		dataT parent;

		//inline bool operator<(Astar_node &a)
		//{return f<a.f;}

		inline bool operator==(Astar_node &a)
		{return data==a.data;}

		inline bool operator<(Astar_node &a)
		{return data<a.data;}

		inline bool operator>(Astar_node &a)
		{return data>a.data;}
	};
//private:
/*
	list <Astar_node> open;
	list <Astar_node> closed;
/*/	array <Astar_node> open;
	array <Astar_node> closed;/**/

	//binary_tree_allocator (Astar_node) openALR;
	//list_allocator (Astar_node) closedALR;
//	list_allocator (Astar_node) ALR;
public:
	array <dataT> way;//Way from goal to start

/*	Astar(dword openNodes=1000,dword closedNodes=10000,dword initPathLen=500)
		:openALR(openNodes),closedALR(closedNodes),open(openALR),closed(closedALR),way(initPathLen)
	{
	}*/
/*
	Astar(dword initNodes=10000,dword initPathLen=500)
		:ALR(initNodes),open(ALR),closed(ALR),way(initPathLen)
	{
	}
/*/
	Astar(dword openNodes=1000,dword closedNodes=20000,dword initPathLen=500)
		:open(openNodes),closed(closedNodes),way(initPathLen)
	{
	}/**/

	void neighbour(Astar_node &n,dataT &ne)
	{
		if (ne==n.data) {iWarning("Bad neighbour"); return;}

		float newg=n.g+n.data.cost(ne);

		Astar_node ne_;
		ne_.data=ne;

		int inO,inC;
		if ((inO=iBinarySearch(open.size,open.data,ne_))>=0)
			if (open[inO].g<=newg) return;

		if ((inC=iBinarySearch(closed.size,closed.data,ne_))>=0)
			if (closed[inC].g<=newg) return;

		ne_.parent=n.data;
		ne_.g=newg;
		ne_.h=ne_.data.goal_dist_estimate();
		ne_.f=ne_.g+ne_.h;

		if (inC>=0) closed.del_shift(inC);
		if (inO>=0) open[inO]=ne_;
		else open.insert(-1-inO,ne_);
	}

	bool search_path(dataT &start,void (*neighbours)(Astar <dataT>::Astar_node &n),bool (*break_search)()=0)
	{
		dword i;
		float min_h;

		way.size=0;

		Astar_node s,n;
		s.data=start;
		s.g=0;
		s.h=s.data.goal_dist_estimate();
		s.f=s.g+s.h;
		s.parent=start;
		open.add(s);

		do
		{
			dword min=0;
			float min_f=open[0].f;

			for (i=1;i<open.size;i++)
				if (open[i].f<min_f)
				{
					min_f=open[i].f;
					min=i;
				}

			n=open[min];
			open.del_shift(min);

			if (n.data.is_goal())//construct path
			{
				if (closed.size==0)
				{
					open.size=0;
					return false;
				}

				goto construct_path;
			}

			neighbours(n);

			int inC;
			if ((inC=iBinarySearch(closed.size,closed.data,n))>=0)
				iWarning("Find duplicate node");
			else closed.insert(-1-inC,n);

			if (break_search)
			if (break_search())
			{
			//	open.size=0;

				min_h=FLT_MAX;

				for (i=0;i<open.size;i++)
					if (open[i].h<min_h)
					{
						min_h=open[i].h;
						n=open[i];
					}
				for (i=0;i<closed.size;i++)
					if (closed[i].h<min_h)
					{
						min_h=closed[i].h;
						n=closed[i];
					}

				goto construct_path;
				//break;
			}
		} while (open.size);

	//	closed.size=0;

		min_h=FLT_MAX;

		for (i=0;i<closed.size;i++)
			if (closed[i].h<min_h)
			{
				min_h=closed[i].h;
				n=closed[i];
			}

construct_path:
		//way.size=0;

		Astar_node t=n,f;
		while (!(t.data==start))//t.parent))
		{
			way.add(t.data);
			f.data=t.parent;
			int in;
			if ((in=iBinarySearch(closed.size,closed.data,f))<0)
			{
				iWarning("Cant find parent_1");
				if ((in=iBinarySearch(open.size,open.data,f))<0)
					iWarning("Cant find parent");
			}
			else t=closed[in];
		}

		open.size=0;
		closed.size=0;

		return (way.size) ? true : false;
	}

#if 0
	void neighbour(Astar_node &n,dataT &ne)
	{
		if (ne==n.data) {iWarning("Bad neighbour"); return;}

		float newg=n.g+n.data.cost(ne);

		Astar_node ne_;
		ne_.data=ne;
//*
		list <Astar_node>::iterator itO,itC;
		bool in_open,in_closed;

		if (in_open=open.find(ne_,itO))
			if (itO->g<=newg) return;

		if (in_closed=closed.find(ne_,itC))
			if (itC->g<=newg) return;
/*/

		int inO,inC;
		if ((inO=iBinarySearch(open.size,open.data,ne_))>=0)
			if (open[inO].g<=newg) return;

		if ((inC=iBinarySearch(closed.size,closed.data,ne_))>=0)
			if (closed[inC].g<=newg) return;
		/**/

		ne_.parent=n.data;
		ne_.g=newg;
		ne_.h=ne_.data.goal_dist_estimate();
		ne_.f=ne_.g+ne_.h;
//*
		if (in_closed) closed.del(itC);//if n' is in Closed remove it from Closed
		if (in_open) open.del(itO);//(*itO) more far from start than ne_
		open.push(ne_);//if n' is not yet in Open push n' on Open
		/*/
		if (inC>=0) closed.del_shift(inC);
		if (inO>=0) open[inO]=ne_;
		else open.insert(-1-inO,ne_);
		/**/
	}

	bool search_path(dataT &start,void (*neighbours)(Astar <dataT>::Astar_node &n),bool (*break_search)()=0)
	{
		//dword i;
//				open.clear();
//				closed.clear();
		float min_h;

		way.size=0;

		Astar_node s,n;
		s.data=start;
		s.g=0;
		s.h=s.data.goal_dist_estimate();
		s.f=s.g+s.h;
		s.parent=start;
		open.push(s);
//		open.add(s);

		do
		{
//*
			list <Astar_node>::iterator2 i=open,min=open;
			float min_f=i->f;
			i++;
			for (;i;i++)
				if (i->f<min_f)
				{
					min_f=i->f;
					min=i;
				}

			n=*min;
			open.del(min);/*/
			dword min=0;
			float min_f=open[0].f;

			for (i=1;i<open.size;i++)
				if (open[i].f<min_f)
				{
					min_f=open[i].f;
					min=i;
				}

			n=open[min];
			open.del_shift(min);
			/**/

			if (n.data.is_goal())//construct path
			{
//				if (closed.size==0)
				if (!closed)
					return false;

				goto construct_path;
			}

			neighbours(n);

//*
			closed.push(n);
/*/			int inC;
			if ((inC=iBinarySearch(closed.size,closed.data,n))>=0)
				iWarning("Find duplicate node");
			else closed.insert(-1-inC,n);
/**/
			if (break_search)
			if (break_search())
			{
				//open.clear();
			//	open.size=0;

				min_h=FLT_MAX;
//*
				list <Astar_node>::iterator i;
				for (i=open;i;i++)
					if (i->h<min_h)
					{
						min_h=i->h;
						n=*i;
					}
				for (i=closed;i;i++)
					if (i->h<min_h)
					{
						min_h=i->h;
						n=*i;
					}
/*/				for (i=0;i<open.size;i++)
					if (open[i].h<min_h)
					{
						min_h=open[i].h;
						n=open[i];
					}
				for (i=0;i<closed.size;i++)
					if (closed[i].h<min_h)
					{
						min_h=closed[i].h;
						n=closed[i];
					}
/**/
				goto construct_path;
				//break;
			}
		} while (open);//.size)//open)

		//closed.clear();
	//	closed.size=0;

		min_h=FLT_MAX;
//*
		list <Astar_node>::iterator i;
		for (i=closed;i;i++)
			if (i->h<min_h)
			{
				min_h=i->h;
				n=*i;
			}
/*/		for (i=0;i<closed.size;i++)
			if (closed[i].h<min_h)
			{
				min_h=closed[i].h;
				n=closed[i];
			}/**/

construct_path:
		//way.size=0;

		Astar_node t=n,f;
		while (!(t.data==start))//t.parent))
		{
			way.add(t.data);
			f.data=t.parent;
		//*
			list <Astar_node>::iterator it;
			if (!closed.find(f,it))
				if (!open.find(f,it)) iWarning("Cant find parent");
			t=*it;/*/

			int in;
			if ((in=iBinarySearch(closed.size,closed.data,f))<0)
			{
				iWarning("Cant find parent_1");
				if ((in=iBinarySearch(open.size,open.data,f))<0)
					iWarning("Cant find parent");
			}
			else t=closed[in];
			/**/
		}

		open.clear();
		closed.clear();
		//open.size=0;
		//closed.size=0;

		return true;
	}
#endif
};
