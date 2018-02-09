char *ToOem(char *text)
{
	static char temp[1000];
	CharToOem(text,temp);
	return temp;
}

class Astar
{
public:
	struct Astar_node
	{
		int data;
		float g;
		int parent;
	};

	float path_len;

	vector <Astar_node> open,closed;
	vector <int> way;//Way from goal to start

	void neighbour(float **graph,Astar_node &n,int ne)
	{
//		if (ne==n.data) {MessageBox(0,"Bad neighbour","Error",0); return;}

		float newg=n.g+graph[n.data][ne];

		Astar_node ne_;
		ne_.data=ne;

		vector <Astar_node>::iterator itO=open.begin(),itC=closed.begin();
		bool in_open=false,in_closed=false;

		for (;itO!=open.end();itO++)
		{
			if (itO->data==ne)
			{
				if (itO->g<=newg) return;
				in_open=true;
				break;
			}
		}

		for (;itC!=closed.end();itC++)
		{
			if (itC->data==ne)
			{
				if (itC->g<=newg) return;
				in_closed=true;
				break;
			}
		}

		ne_.parent=n.data;
		ne_.g=newg;

		if (in_closed) closed.erase(itC);//if n' is in Closed remove it from Closed
		if (in_open) open.erase(itO);//(*itO) more far from start than ne_
		open.push_back(ne_);//if n' is not yet in Open push n' on Open
	}

	bool search_path(int numV,float **graph,int start,int end)
	{
		open.clear();
		closed.clear();
		way.clear();

		Astar_node s,n;
		s.data=start;
		s.g=0;
		s.parent=start;
		open.push_back(s);
	//		open.add(s);

		do
		{
			vector <Astar_node>::iterator i=open.begin(),min=open.begin();
			float min_g=i->g;
			i++;
			for (;i!=open.end();i++)
				if (i->g<min_g)
				{
					min_g=i->g;
					min=i;
				}

			n=*min;
			open.erase(min);

			if (n.data==end)//construct path
			{
	//				if (closed.size==0)
				if (closed.empty())
					return false;

				path_len=n.g;

				goto construct_path;
			}

			//neighbours
			for (int ii=0;ii<numV;ii++)
			{
				if (graph[n.data][ii])
					neighbour(graph,n,ii);
			}

			closed.push_back(n);
		} while (!open.empty());

		//closed.clear();
	//	closed.size=0;

		return false;


	construct_path:
		//way.size=0;

		Astar_node t=n;
		while (!(t.data==start))//t.parent))
		{
			way.push_back(t.data);
			vector <Astar_node>::iterator it;
			for (it=closed.begin();it!=closed.end();it++)
				if (it->data==t.parent)
					break;

			if (it==closed.end())
			{
				for (it=open.begin();it!=open.end();it++)
					if (it->data==t.parent)
						break;

				if (it==open.end())
					terminate();
//					MessageBox(0,"Cant find parent","Error",0);
			}

			t=*it;
		}

		open.clear();
		closed.clear();

		if (!way.empty())
		{
			way.push_back(start);
			return true;
		}
		else return false;
	}
};

class graph
{
public:
	int numV;
	float **M;

	graph()
	{
		int i;
		char or;

		cout<<ToOem("Введите количество вершин: ");
		cin>>numV;

		M=new float* [numV];

		for (i=0;i<numV;i++)
		{
			M[i]=new float [numV];

			for (int j=0;j<numV;j++)
				M[i][j]=0;
		}

		cout<<ToOem("Граф ориентированный? (Y - да): ");
		cin>>or;
		if (or!='Y' && or!='y') or=0;

		int begin,end;

		while (1)
		{
			cout<<ToOem("Дуга: (начало, конец, длина) > ");
			float len;
			cin>>begin>>end>>len;
			if (begin==0 || end==0) break;
			begin--; end--;

			if (begin<0 || begin>=numV || end<0 || end>=numV || len<=0 || begin==end)
			{
				cout<<ToOem("Ввод не принят\n");
				continue;
			}

			M[begin][end]=len;
			if (!or)
				M[end][begin]=len;
		}
	}

	~graph()
	{
		for (int i=0;i<numV;i++)
			delete [] M[i];

		delete [] M;
	}

	bool search_path(Astar &astar,int begin,int end)
	{
		return astar.search_path(numV,M,begin,end);
	}
};
