/*!
\file
\brief Реализация библиотеки построения мостиков 

\author Владимир Иноземцев
\version 1.0
*/

#ifndef RPC_BRIDGES_DEV_H
#define RPC_BRIDGES_DEV_H

//std
#include <memory>
#include <iostream>
#include <utility>
#include <algorithm>
#include <numeric>
#include <fstream>
//gdal
#include <gdal_priv.h>
#include <ogrsf_frmts.h>
//boost
#include <boost/config.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/prim_minimum_spanning_tree.hpp>
#include <boost/graph/kruskal_min_spanning_tree.hpp>
#include <boost/graph/graph_traits.hpp>

#include <boost/serialization/map.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
//использовать только совместно (баг boost 1.6.x)
#include <boost/type_traits/ice.hpp>
#include <boost/graph/adjacency_matrix.hpp>
//мои модули
#include <clasterutils.h>
#include <gdalutilities.h>
#include <tiles.h>

/*!
\brief Структура хранения данных о графе - списки смежности.
\details Явно занимает меньше памяти, чем матрица смежности.
*/
#define BRIDGES_LIST
/*
\brief Cтруктура хранения данных о графе - матрица смежности.
\details Занимает больше места, чем списки смежности. По-идее,
должна работать быстрее. Но по факту не работает.
*/
#undef BRIDGES_MATR

/*!
Использовать для расчета расстояния между точками мой велосипедный
метод. Он в 70-80 раз быстрее стандартного расчета GDAL, однако
может не учитывать сферическую геометрию
*/
#define BRIDGES_STUB_DISTANCE

/*!
Использовать для расчета расстояния стандартный метод GDAL
*/
#undef BRIDGES_GDAL_DISTANCE

/*!
Этот макрос включает или выключает все операции Union GDAL/OGR.
Отключение этих операций нужно для ускорения отладки.
*/
#undef BRIDGES_UNION

namespace BridgesRPC
{
    using namespace GDALUtilities::Boilerplates;
    using std::shared_ptr;
    using Tiles::Tile;
    using Tiles::TileCollection;
    ///свойство ребер - вес (для нас - расстояние)
    typedef boost::property<boost::edge_weight_t,
        double> EdgeWeightProperty;
#ifdef BRIDGES_LIST
    ///граф представлен в виде списка смежности
    typedef boost::adjacency_list<
        boost::vecS, //вершины хранятся в std::vector
        boost::vecS, //ребра хранятся в std::vector
        boost::undirectedS, //неориентированный граф
        boost::no_property, //у вершин нет свойств
        EdgeWeightProperty>
        BridgeGraph;
#endif
#ifdef BRIDGES_MATR
    ///граф представлен в виде матрицы смежности
    typedef boost::adjacency_matrix<
        boost::undirectedS, //неориентированный граф
        boost::no_property, //у вершин нет свойств
        EdgeWeightProperty>
        BridgeGraph;
#endif
    ///дескриптор ребер
    typedef BridgeGraph::edge_descriptor Edge;
    /*!
    \brief Тип минимального остовного дерева
    \details Вектор дескрипторов ребер.
    Используется для создания мостиков между
    полигонами. В каждом ребре содержится индексы
    двух полигонов.
    */
    typedef std::vector<Edge> MinimumSpanningTree;

    ///существует ли файл
    inline bool FileExists(const std::string &fname);

    /*!
    \brief Запись тестовых данных в файл
    */
    void SaveTmp(std::string fname, OGC *g);

    ///Выводит информацию о геометрии в консоль
    void ExamineGeometry(OGRGeometry *g); 

    /*!
    \brief Автоматический расчет размера сетки
    */
    double CalculateGridSize(OGC* input);

    /*!
    \brief Деление геометрии сеткой
    \param[in] input Массив геометрий. Не допускается nullptr.
    \param[in] grid Сетка, сгенерированная generageGrid
    \param[in] map Структура, которая содержит флаги "соединяемости"
    полигонов
    */
    TileCollection *SplitGeometryByGrid
        (OGRGeometryCollection *input, OGRGeometryCollection *grid,
            bool verbose = false);

    /*!
    \brief Велосипедный расчет расстояния между
    полигонами
    \param[in] a Полигон 1. Не допускается nullptr.
    \param[in] b Полигон 2. Не допускается nullptr.
    */
    double DistanceBetweenPolygons(OGRPolygon *a, OGRPolygon *b);

    /*!
    \brief Создание графа для исходной геометрии
    \details Возвращает новый граф, вершины которого
    соответствуют точкам будущих мостиков. Ребра строятся, если
    расстояние между точками не превышает dist. Это нужно
    для сокращения потребления памяти.
    \details Без оптимизации граф должен был бы содержать ребра,
    соединяющие все геометрии со всеми. Но произведена оптимизация,
    чтобы не соединять ребрами далекие друг от друга точки.
    \param[in] tiles Исходная коллекция тайлов. Не допускается
    nullptr
    \param[in] max_distance Максимальная дистанция, для которой между
    тайлами может быть построен мостик
    \param[in] verbose Если true, то функция пишет в консоль отладочную
    информацию.
    \return Новый граф
    */
    BridgeGraph *CreateGraph(TileCollection *tiles, 
        double max_distance, bool verbose = false);

    /*!
    \brief Создание минимального остовного дерева.
    \details Расчет происходит с использованием алгоритма Краскала
    из библиотеки boost.
    \param[in] g Инициализированное дерево. Не допускается null.
    \return Новое минимальное остовное дерево
    */
    MinimumSpanningTree *KruskalMST(BridgeGraph *g, bool verbose = false);

    /*!
    \brief Мостик выпуклой оболочкой
    \details Расчет геометрии мостика между двумя
    полигонами при помощи выпуклой оболочки.
    \param[in] p1 Полигон 1. Не допускается nullptr.
    \param[in] p2 Полигон 2. Не допускается nullptr.
    \return Новый мостик-полигон между двух полигонов
    в случае успеха, nullptr при ошибке.
    */
    OGRPolygon *BridgeWithConvexHull(OGRPolygon *p1,
        OGRPolygon *p2);

    /*!
    \brief Мостик буферизованным отрезком
    \details Расчет геометрии мостика между двумя
    полигонами при помощи буферизованного на определенную
    величину отрезка.
    \param[in] p1 Полигон 1. Должен быть меньшим из двух.
    Не допускается nullptr.
    \param[in] p2 Полигон 2. Должен быть бОльшим из двух.
    Не допускается nullptr.
    \return Новый мостик-полигон между двух полигонов
    в случае успеха, nullptr при ошибке.
    */
    OGRPolygon *BridgeWithBufferedLine(OGRPolygon *p1,
        OGRPolygon *p2);

    /*!
    \brief Автоматическое создание мостика
    \details Рассчитывается геометрия мостика между
    двумя полигонами более предпочтительным методом.
    Метод выбирается на основании отношения площадей
    полигонов. Если отношение площади большего полигона
    к площади меньшего больше ratio, то мостик создается
    буферизованным отрезком. В противном случае мостик
    создается выпуклой оболочкой.
    \param[in] p1 Полигон 1. Не допускается nullptr.
    \param[in] p2 Полигон 2. Не допускается nullptr.
    \param[in] counter_ch Указатель на счетчик операций
    соединения полигонов выпуклыми оболочками.
    \param[in] counter_br Укзатель на счетчик операций
    соединения полигонов мостиками.
    \return Новый мостик-полигон между двух полигонов
    в случае успеха, nullptr при ошибке.
    */
    OGRPolygon *AutoBridge(OGRPolygon *p1,
        OGRPolygon *p2, size_t *counter_ch = nullptr,
        size_t *counter_br = nullptr);


    /*!
    \brief Автоматическое создание мостиков по
    минимальному остовному дереву
    \details Создается новая геометрическая коллекция,
    элементы которой являются мостиками между tiles
    и соответствующему им минимальному остовному
    дереву tree.
    \param[in] tiles Коллекция тайлов. Не допускается
    nullptr.
    \param[in] tree Инициализированное минимальное остовное
    дерево. Не допускается nullptr.
    \return Геометрию мостиков. nullptr в случае ошибки.
    */
    OGC *CreateBridgesByTree(MinimumSpanningTree *tree, 
        TileCollection *tiles);


    /*!
    \brief Класс - конструктор мостиков.
    */
    class BridgeConstructor
    {
    private:

        /*!число мостиков, построенных с использованием
        convex hull*/
        size_t conv_hull_bridges;

        /*!
        число мостиков, построенных с использованием
        буферизованных отрезков*/
        size_t buf_line_bridges;

        OGRMultiPolygon *input; ///<Исходная геометрия.

                                /*!
                                \brief Максимальное отношение площади
                                большего полигона к площади меньшего для выбора
                                алгоритма построения мостика.
                                */
        double ratio;

        /*!
        \brief Максимальное расстояние между полигонами,
        для которого строится мостик.
        */
        double max_distance;

        /*!
        \brief Определение, соразмерны ли два полигона
        по площади
        */
        bool closeArea(OGRPolygon *a, OGRPolygon *b);

    public:
        /*!
        \brief Конструктор
        \param[in] input Исходная геометрия. Не допускается nullptr
        \param[in] ratio Максимальное отношение площади
        большего полигона к площади меньшего для выбора
        алгоритма построения мостика.
        \param[in] max_distance Максимальное расстояние между полигонами,
        для которого строится мостик. В данной версии BridgesRPC расстояние
        между полигонами вычисляется как расстояние между центроидами.
        */
        explicit BridgeConstructor(OGRGeometryCollection* _input, double _ratio, double _max_distance);
        /*!
        \brief Конструирование мостиков
        \return Новая коллекция полигонов.
        nullptr при ошибке
        */
        OGC *exec();

        ~BridgeConstructor();
    };

}

#endif
