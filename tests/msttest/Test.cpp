/*
Тест алгоритма нахождения минимального остовного дерева
из библиотеки boost. Минимальное остовное дерево соединяет
вершины графа так, что суммарный вес дерева минимальный.

В программе при компиляции граф можно представить как в
виде списка смежности (ADJ_LIST), так и в виде матрицы
смежности (ADJ_MATRIX)
*/
#include <boost/config.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/prim_minimum_spanning_tree.hpp>
#include <boost/graph/kruskal_min_spanning_tree.hpp>
#include <boost/graph/graph_traits.hpp>

#include <iostream>
#include <utility>
#include <algorithm>

//использовать только совместно
#include <boost/type_traits/ice.hpp>
#include <boost/graph/adjacency_matrix.hpp>

//#define ADJ_MATRIX
#define ADJ_LIST

using namespace boost;

//свойство ребер - вес (для нас - расстояние)
typedef property<edge_weight_t, double> EdgeWeightProperty;
#ifdef ADJ_LIST
//граф представлен в виде списка смежности
typedef boost::adjacency_list<vecS,vecS,undirectedS,no_property,EdgeWeightProperty> mygraph;
#endif
#ifdef ADJ_MATRIX
//граф представлен в виде матрицы смежности
typedef boost::adjacency_matrix<undirectedS, no_property, EdgeWeightProperty> mygraph;
#endif

typedef mygraph::edge_descriptor Edge;

struct Point2D
{
	float x;
	float y;
};

//случайное число в интервале
float RandomNum(float min, float max)
{
	return
		min + static_cast<float>(rand()) 
		/ (static_cast<float>(RAND_MAX) / (max - min));
}

//случайная точка
Point2D RandomPoint(float xmin,float ymin,float xmax,float ymax)
{
	Point2D p;
	p.x = RandomNum(xmin, xmax);
	p.y = RandomNum(ymin, ymax);
	return p;
}

//расстояние между точками
float Distance(Point2D a, Point2D b)
{
	float dx = a.x - b.x;
	float dy = a.y - b.y;
	return sqrt(dx*dx + dy*dy);
}

int main()
{
	using std::cout;

	//число точек
	const int pnum = 600;
	//граничное расстояние между точками
	//если расстояние между точками меньше max_dst,
	//то между точками создается ребро графа,
	//иначе нет. Нужно для оптимизации расхода памяти программы
	const float max_dst = 10.0;
	//создаем массив из хаотично расположенных точек
	Point2D points[pnum];
	for (int i = 0;i < pnum;i++)
		points[i] = RandomPoint(-100.0, -100.0, 100.0, 100.0);

	mygraph g(pnum);

	//в граф записываем все хаотично расположенные точки
	for (int i = 0;i < pnum;i++)
		for (int j = 0;j < pnum;j++)
		{
			float dst = Distance(points[i], points[j]);
			if (dst < max_dst)
				add_edge(i, j, Distance(points[i], points[j]), g);
		}


	cout << "number of edges: " << num_edges(g) << std::endl;
	cout << "number of vertices: " << num_vertices(g) << std::endl;
	
	//обход графа

	//находим минимальное остовное дерево
	//оно в виде списка пар вершин
	std::list<Edge> spanning_tree;
	kruskal_minimum_spanning_tree(g, std::back_inserter(spanning_tree));
	
	cout << "spanning tree length: " << spanning_tree.size() << std::endl;

	for (std::list<Edge>::iterator i = spanning_tree.begin();
	i != spanning_tree.end();i++)
	{
		//достаем индексы вершин из spanning_tree
		Edge e = *i;
		//вершина, из которой идет ребро
		size_t source_index = e.m_source;
		//вершина, в которую идет ребро
		size_t dest_index = e.m_target;
		//просто выводим индексы, хотя можем делать что-то полезное
		//например, строить мостики между полигонами с заданными индексами
		printf("src = %zd target = %zd\n", source_index, dest_index);
	}
	cout << std::endl;

	getc(stdin);
	return 0;
}
