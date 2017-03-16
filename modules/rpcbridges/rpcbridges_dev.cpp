#include "rpcbridges_dev.h"

using namespace BridgesRPC;

bool BridgesRPC::FileExists(const std::string &fname)
{
    if (FILE *file = fopen(fname.c_str(), "r"))
    {
        fclose(file);
        return true;
    }
    else
        return false;
}

void BridgesRPC::SaveTmp(std::string fname,OGC *g)
{
    assert(g);
    if (g->IsEmpty()) return;
    auto gtype = g->getGeometryRef(0)->getGeometryType();
    //Записываем полученные полигоны
    GDALDataset *outDataset = GDALUtilities::OpenSHPFile(fname);
    if (!outDataset)
    {
        printf("Could not write to SHP file!\n");
        printf("Close file in QGIS!!!\n");
        return;
    }
    assert(outDataset != nullptr);
    OGRLayer* outLayer = outDataset->CreateLayer("testdata", 0, gtype, 0);
    assert(outLayer);
    if (!outLayer)
    {
        printf("Could not create layer in SHP file!\n");
        return;
    }
    for (int i = 0;i < g->getNumGeometries();i++)
    {
        //создаем feature с геометрией
        OGRFeature *outputFeature;
        outputFeature = OGRFeature::CreateFeature(outLayer->GetLayerDefn());
        if (!outputFeature)
        {
            printf("Could not create test data!\n");
            return;
        }
        assert(outputFeature != nullptr);
        outputFeature->SetGeometry(g->getGeometryRef(i));
        //записываем фичу на диск
        if (outLayer->CreateFeature(outputFeature) != OGRERR_NONE)
        {
            //не смогли записать фичу, аварийно завершаем программу
            printf("failed to write feature to output file!\n");
            OGRFeature::DestroyFeature(outputFeature);
            GDALClose(outDataset);
        }

        OGRFeature::DestroyFeature(outputFeature);
    }

    GDALClose(outDataset);
}

void BridgesRPC::ExamineGeometry(OGRGeometry * g)
{
    char *str;
    g->exportToWkt(&str);
    std::cout << str << std::endl;
    OGRFree(str);
}

double BridgesRPC::CalculateGridSize(OGC * input)
{
    //геометрия не должна быть nullptr!
    assert(input);

    //ничего не делаем для пустой геометрии
    if (input->getNumGeometries() == 0) return 0.0;

    std::vector<double> bbarea;
    //считаем стороны bounding box'ов полигонов
    for (int i = 0;i < input->getNumGeometries();i++)
    {
        OGREnvelope env;
        input->getGeometryRef(i)->getEnvelope(&env);
        double dx = env.MaxX - env.MinX;
        double dy = env.MaxY - env.MinY;
        bbarea.push_back(dx*dy);
    }
    //считаем среднюю длину стороны bb
    double avg_len =
        std::accumulate(bbarea.begin(), bbarea.end(), 0.0);
    avg_len /= static_cast<double>(bbarea.size());
    //делим пополам (???????)
    avg_len = sqrt(avg_len) / 2.0;

    return avg_len;
}

bool BridgeConstructor::closeArea(OGRPolygon *a,OGRPolygon *b)
{
    //проверяем входные данные
    assert(a);
    assert(b);

    double area_a = a->get_Area();
    double area_b = b->get_Area();

    //считаем площади меньшего и большего полигонов
    double max_area = std::max(area_a, area_b);
    double min_area = std::min(area_a, area_b);

    //отношение площадей
    double r = max_area / min_area;

    //возвращаем true, если их отношение меньше ratio
    return (r < ratio);
}

double BridgesRPC::DistanceBetweenPolygons(OGRPolygon *a, OGRPolygon *b)
{
    //проверяем входные данные
    assert(a);
    assert(b);

    //расстояние между ними
    //примерно равно расстоянию между центроидами
#ifdef BRIDGES_GDAL_DISTANCE
        //расстояние, которое считает gdal, медленное, но корректное
       return pi->Distance(pj);
#endif 
#ifdef BRIDGES_STUB_DISTANCE
        //считаем самостоятельно (велосипед)
        OGRPoint *ppi = GDALUtilities::FailsafeCentroid(a);
        OGRPoint *ppj = GDALUtilities::FailsafeCentroid(b);
        assert(ppi != nullptr);
        assert(ppj != nullptr);
        double dx = ppi->getX() - ppj->getX();
        double dy = ppi->getY() - ppj->getY();
        return sqrt(dx*dx + dy*dy);
#endif
}

BridgeGraph * BridgesRPC::CreateGraph(TileCollection * tiles, double max_distance, bool verbose)
{
    using std::cout;
    
    BridgeGraph *graph;

    // создаем новый объект graph
#ifdef BRIDGES_LIST
    graph = new BridgeGraph();
#endif
#ifdef BRIDGES_MATR
        //для матрицы нужно указывать явно число вершин
    graph = new BridgeGraph(centroids->getNumGeometries());
#endif

    assert(tiles);

    //данные прогресса
    GDALUtilities::ProgressIndicator
        indicator(tiles->size()*tiles->size() / 2,
            "BridgesRPC::CreateGraph");

    /*перебираем все исходные геометрии
    считаем расстояние между ними
    добавляем ребра*/
    for (auto i = tiles->begin();i != tiles->end();i++)
    {
        //j указывает на следующий элемент
        auto j = i;
        j++;
        for (;j != tiles->end();j++)
        {
            //обновляем прогрессбар
            if (verbose)
                indicator.incOperationCount();

            //тайлы
            auto ti = (*i).second;
            auto tj = (*j).second;

            assert(ti);
            assert(tj);

            auto pi = dynamic_cast<OGRPolygon*>
                (ti->geometry());
            auto pj = dynamic_cast<OGRPolygon*>
                (tj->geometry());

            //если не удалось преобразование типов,
            //пропускаем цикл
            if (!pi || !pj) continue;

            //расстояние между полигонами
            double distBetween =
                DistanceBetweenPolygons(pi, pj);

            //ошибки вычисления расстояния не должно быть!
            //distBetween = -1, если произошла ошибка GEOS
            assert(distBetween > 0);

            //проверяем расстояние
            if (distBetween <= max_distance)
                //оно меньше заданного
            {
                //проверяем группы
                //они не должны совпадать
                if (tiles->inSameGroup(ti->index(), tj->index()))
                    continue;
                //полигоны из разных групп
                //можно создавать мостики.
                //создаем ребро
                add_edge(ti->index(), tj->index(), 
                    distBetween, *graph);
            }

            if (verbose)
            {
                //печатаем пользователю информацию о созданном графе
                cout << "\nBridgesRPC::CreateGraph info: " << std::endl;
                cout << "\tnumber of edges: " << num_edges(*graph) << std::endl;
                cout << "\tnumber of vertices: " << num_vertices(*graph) << std::endl;
            }
        }
    }

                return graph;
}

MinimumSpanningTree *BridgesRPC::KruskalMST(BridgeGraph *g, bool verbose)
{
    using std::cout;

    cout << "BridgesRPC::KruskalMST\n";

    //не принимаем неинициализированный граф
    assert(g != NULL);

    //инициализируем новое минимальное остовное дерево
    MinimumSpanningTree *tree = new MinimumSpanningTree();

    //запускаем алгоритм Краскала из boost
    kruskal_minimum_spanning_tree(*g, std::back_inserter(*tree));

    if (verbose)
    {
        //печатаем результаты алгоритма для пользователя
        cout << "BridgesRPC::KruskalMST info: " << std::endl;
        cout << "\tspanning tree length: " <<
            tree->size() << std::endl;
    }

    return tree;
}

OGRPolygon * BridgesRPC::BridgeWithConvexHull(OGRPolygon * p1, OGRPolygon * p2)
{
    //полигоны не должны быть nullptr
    assert(p1 != nullptr);
    assert(p2 != nullptr);
    
    //создаем коллекцию геометрий из p1 и p2
    //выпуклая оболочка строится между геометриями
    //только если они в коллекции
    //не используется за пределами функции, поэтому shared ptr
    TempOMP collection(newMultiPolygon(), destroy);
    //Не должно быть ошибки приведения OGRGeometry к OGC
    assert(collection.get() != nullptr);

    //добавляем в коллекцию копии полигонов
    collection->addGeometry(p1);
    collection->addGeometry(p2);

    //создаем выпуклую оболочку
    std::shared_ptr<OGRPolygon>
        bridge(dynamic_cast<OGRPolygon*>
            (collection->ConvexHull()),
            OGRGeometryFactory::destroyGeometry);
    //Не должно быть ошибки приведения OGRGeometry к OGC
    assert(bridge != nullptr);

    //Возвращаем копию мостика
    OGRPolygon* result =
        dynamic_cast<OGRPolygon*>
        (bridge->clone());

    //Не должно быть ошибки приведения OGRGeometry к OGRPolygon
    assert(result != nullptr);

    //Возвращаем новый мостик
    return result;
}

OGRPolygon * BridgesRPC::BridgeWithBufferedLine(OGRPolygon * p1, OGRPolygon * p2)
{
    //полигоны не должны быть nullptr
    assert(p1 != nullptr);
    assert(p2 != nullptr);

    //считаем размер буферной зоны
    double buf_sz =
        std::min(GDALUtilities::InscribedCircleRadius(p1),
            GDALUtilities::InscribedCircleRadius(p2));

    //не смогли вычислить размер буферной зоны, ничего не возвращаем
     if (fabs(buf_sz) < 1e-6)
         return nullptr;

    //считаем центроиды полигонов
    std::shared_ptr<OGRPoint>
        c1( GDALUtilities::FailsafeCentroid(p1),
            OGRGeometryFactory::destroyGeometry);
    std::shared_ptr<OGRPoint>
        c2(GDALUtilities::FailsafeCentroid(p2),
            OGRGeometryFactory::destroyGeometry);

    //не должна происходить ошибка приведения типов!
    assert(c1.get() != nullptr);
    assert(c2.get() != nullptr);

    //создаем отрезок
    std::shared_ptr<OGRLineString>
        ls(dynamic_cast<OGRLineString*>
            (OGRGeometryFactory::createGeometry(wkbLineString)),
            OGRGeometryFactory::destroyGeometry);
    //добавляем в него две точки
    ls->addPoint(c1.get());
    ls->addPoint(c2.get());

    //буферизуем его
    OGRPolygon *res =
        dynamic_cast<OGRPolygon*>
        (ls->Buffer(buf_sz));
    //не должно быть ошибок приведения типа
    assert(res != nullptr);

    //возвращаем новый полигон
    return res;
}

OGRPolygon * BridgesRPC::AutoBridge(OGRPolygon *p1,OGRPolygon *p2, size_t *counter_ch,
    size_t *counter_br)
{
    //индексы должны быть валидны
    assert(p1);
    assert(p2);
    //полигон - мостик
    OGRPolygon *res;

    //массив из 2-х индексов
    std::vector<OGRPolygon*> p;
    p.push_back(p1);
    p.push_back(p2);
    //сортируем массив по возрастанию площади
    //p[0] - меньший полигон, p[1] - больший
    std::sort(p.begin(), p.end(),
        [](OGRPolygon *a,OGRPolygon *b)
    {return a->get_Area() < b->get_Area();});

    
    //BridgeWithConvexHull - пока не используется

    res = BridgeWithBufferedLine(p1, p2);
    if (counter_br)
        *counter_br++;

    return res;
}

OGC *BridgesRPC::CreateBridgesByTree
(MinimumSpanningTree *tree,TileCollection *tiles)
{
    //данные прогресса
    assert(tree);
    GDALUtilities::ProgressIndicator
        indicator(tree->size(), "BridgesRPC::CreateBridgesByTree");

    assert(tiles);
    
    /*создаем новую коллекцию полигонов
    Почему не OGRMultiPolygon? Ведь в коллекции только полигоны.
    Проблема в том, что OGRMultiPolygon не допускает пересечения
    полигонов, а OGC допускает.
    */
    auto bridges = newGeometryCollection();
    //не должно быть ошибки при приведении типов
    //из OGRGeometry в OGRGeometryFactory
    assert(bridges != nullptr);

    //перебираем дерево, создаем мостики
    for (auto i = tree->begin();i != tree->end();i++)
    {
#ifndef BRIDGES_DEBUG
        //обновляем прогрессбар
        indicator.incOperationCount();
#endif
        //текущее ребро
        Edge e = *i;
        //не должно быть переполнения переменных индекса
        int m_source = static_cast<int>(e.m_source);
        int m_target = static_cast<int>(e.m_target);
        assert(m_source == e.m_source);
        assert(m_target == e.m_target);

        //соответствующие тайлы
        auto srcTile = tiles->tileByIndex(m_source);
        auto destTile = tiles->tileByIndex(m_target);
        //берем геометрии из коллекции
        OGRGeometry *src = srcTile->geometry();
        OGRGeometry *dst = destTile->geometry();
        //они не должны быть nullptr
        assert(src);
        assert(dst);
        //проверяем, что это полигоны
        if (src->getGeometryType() != wkbPolygon ||
            dst->getGeometryType() != wkbPolygon) continue;
        //преобразуем геометрии
        OGRPolygon *p_src = static_cast<OGRPolygon*>
            (src);
        OGRPolygon *p_dst = static_cast<OGRPolygon*>
            (dst);
        //создаем геометрию нового мостика
        OGRPolygon *bridge = AutoBridge(p_src,p_dst);
        //может возникнуть ошибка при создании мостика
        if (bridge)
        {
#ifdef BRIDGES_DEBUG
            std::cout << "BRIDGE: Polygon";
            std::cout << srcTile->index() << " from group ";
            std::cout << srcTile->group();
            std::cout << " and Polygon" << destTile->index();
            std::cout << " from group " << destTile->group();
            std::cout << std::endl;
#endif
            //геометрия должна быть валидна
            assert(bridge->IsValid());

#ifdef BRIDGES_UNION
                //пересекает ли мостик нашу геометрию?
                if (res->Intersect(bridge))
                {
                    //может случиться самопересечение res, что
                    //недопустимо согласно документации gdal/ogr
                    //поэтому делаем Union
                    OGRMultiPolygon *tmp_geom = dynamic_cast<OGRMultiPolygon*>(res->Union(bridge));
                    //не может получиться nullptr
                    assert(tmp_geom);
                    OGRGeometryFactory::destroyGeometry(res);
                    res = dynamic_cast<OGRMultiPolygon*>(tmp_geom);
                }
                else
                    //просто добавляем новый мостик, поскольку он не пересекается
                    res->addGeometryDirectly(bridge);
#else
            bridges->addGeometryDirectly(bridge);
#endif
        }
        else
        {
            //не удалось создать мостик
            //говорим об этом пользователю
            std::cout << "Error when creating bridge\n";
        }
    }

    return bridges;
}


TileCollection *BridgesRPC::SplitGeometryByGrid
(OGRGeometryCollection *input, OGRGeometryCollection *grid, bool verbose)
{
    assert(grid);
    assert(input);

    //в operated складываем тайлы
    auto tiles = new TileCollection;

    //данные прогресса
    GDALUtilities::ProgressIndicator
        indicator(input->getNumGeometries()*grid->getNumGeometries(), 
            "BridgesRPC::splitInputByGrid");

    int ngeom = input->getNumGeometries();
    //нечего разделять, ничего не возвращаем
    if (ngeom == 0) return nullptr;

    //перебираем исходные полигоны
    for (int i = 0;i < input->getNumGeometries();i++)
    {
        //текущий полигон суши
        auto curInput = input->getGeometryRef(i);

        //текущая геометрия должна быть именно полигоном
        if (curInput->getGeometryType() != wkbPolygon)
            continue;

        //преобразуем его в MultiLineString
        shared_ptr<OGRMultiLineString>
            mls(
                GDALUtilities::ExternalRingToMLS(
                    static_cast<OGRPolygon*>(curInput)
                    ),
                destroy);

        //смотрим, с какими элементами сетки пересекается полигон
        for (int j = 0;j < grid->getNumGeometries();j++)
        {
#ifndef BRIDGES_DEBUG
            //прогресс
            //не работает в debug'e
            if (verbose)
                indicator.incOperationCount();
#endif
            //текущий квадратик сетки
            auto curGrid = grid->getGeometryRef(j);

            //элемент сетки должен пересекать внешнее кольцо полигона!
            if (!curGrid->Intersects(mls.get())) continue;

            //геомертрия пересечения
            auto newGeom = curInput->Intersection(curGrid);
            //проверяем вид этой геометрии
            auto gtype = newGeom->getGeometryType();
            switch (gtype)
            {
            case wkbPolygon:
            {
#ifdef BRIDGES_DEBUG
                //debug вывод в консоль
                ExamineGeometry(newGeom);
#endif
                //создаем новый тайл
                tiles->addTile(new Tile(newGeom,i));
                break;
            }
            case wkbMultiPolygon:
            {
                auto newGeomP = dynamic_cast<OMP*>(newGeom);
                //из мультиполигона берем полигоны и записываем в коллекцию
                assert(newGeomP);
                for (int k = 0;k < newGeomP->getNumGeometries();k++)
                {
                    //полигон из получившегося мультиполигона
                    OGRGeometry *z = newGeomP->getGeometryRef(k);
                    assert(z);
                    //создаем новый тайл
                    tiles->addTile(new Tile(z, i));
                }
                break;
            }
            case wkbLineString:
            {
                //такое возможно, если элемент сетки касается
                //нашей геометрии
                //это не ошибка, но линию в выходную коллекцию
                //добавлять не будем.
#ifdef BRIDGES_DEBUG
                //debug вывод в консоль
                ExamineGeometry(newGeom);
#endif
                break;
            }
            default:
                //такого не должно быть!
                assert(0);
                break;
            }

        }
    }

    return tiles;
}

BridgesRPC::BridgeConstructor::BridgeConstructor(OGRGeometryCollection *_input, double _ratio, double _max_distance)
{
    //проверка входных данных

    //не принимаем nullptr _input
    assert(_input);

    //ratio должен быть больше 1
    if (_ratio <= 1)
    {
        //невозможно выполнить алгоритм
        std::cout << "ratio <= 1, setting ratio to 5" << std::endl;
        ratio = 5;
    }
    else
        ratio = _ratio;
    //max distance не должен быть больше 0
    if (_max_distance <= 0)
    {
        //невозможно выполнить алгоритм
        std::cout << "max distance <= 0, setting it to 0.1" << std::endl;
        max_distance = 0.1;
    }
    else
        max_distance = _max_distance;

    input = dynamic_cast<OGRMultiPolygon*>
        (_input->clone());
}

OGC *BridgeConstructor::exec()
{
    //ничего не делаем для пустой геометрии
    if (input->getNumGeometries() == 0) return nullptr;

    std::vector<double> bbarea;
    //считаем стороны bounding box'ов полигонов
    for (int i = 0;i < input->getNumGeometries();i++)
    {
        OGREnvelope env;
        input->getGeometryRef(i)->getEnvelope(&env);
        double dx = env.MaxX - env.MinX;
        double dy = env.MaxY - env.MinY;
        bbarea.push_back(dx*dy);
    }
    //считаем среднюю длину стороны bb
    double avg_len =
        std::accumulate(bbarea.begin(), bbarea.end(), 0.0);
    avg_len /= static_cast<double>(bbarea.size());
    avg_len = sqrt(avg_len)/2.0;

    std::cout << "grid size = " << avg_len << std::endl;

    shared_ptr<TileCollection> tiles;

    //проверяем, есть ли кэш
    //если мы уже поделили геометрию сеткой, не будем делать это еще раз
#ifdef BRIDGES_CACHING
    if (!FileExists("G:/testmap/test_results/tiles.shp"))
    {
#endif
        //генерируем сетку, которая покрывает полигоны
        TempOGC grid(GDALUtilities::GenerateGrid(input, avg_len), destroy);

        //делим полигоны сеткой
        //TempOGC operated(splitGeometryByGrid(input, grid.get()), dg);

        tiles.reset( SplitGeometryByGrid(input, grid.get()) );

#ifdef BRIDGES_CACHING
        //сохраняем массив в кэш
        std::ofstream f("G:/testmap/test_results/bm.txt", std::ios::binary);
        boost::archive::text_oarchive oarch(f);
        oarch << bm;
        f.close();
#endif

        ///\todo сделать сохранение в файл
        //сохраняем в файл
        //if (tiles.get())
        //    SaveTmp("G:/testmap/test_results/tiles.shp", tiles.get());
#ifdef BRIDGES_CACHING
    }
    else
    {
        //уже поделили сеткой, считываем поделенную геометрию
        GDALUtilities::StringList list;
        list.push_back("G:/testmap/test_results/tiles.shp");
        TempOMP res(GDALUtilities::FetchGeometryFromFiles(list, "tiles"), 
            destroy);

        //считываем структуру с флагами
        std::ifstream f("G:/testmap/test_results/bm.txt", std::ios::binary);
        boost::archive::text_iarchive iarch(f);
        iarch >> bm;
        f.close();
        
        for (int i = 0;i < res->getNumGeometries();i++)
            tiles->addGeometry(res->getGeometryRef(i));
    }
#endif
/*
    int cnt = 1;

    //кластеризуем полигоны
    int nclasters = 10;
    std::shared_ptr<ClasterUtils::GeometryClasters> clasters(
        ClasterUtils::ClasterizeByCentroids(tiles.get(),nclasters));

    OGC *final_bridges = newGeometryCollection();

    //строим мостики в каждом кластере
    cnt = 1;
    for (auto i = clasters->begin();i != clasters->end();i++)
    {
        //сохраняем тайлы, сгрупированные по кластерам
            std::string fname = std::string("G:/testmap/test_results/tiles")
                + std::to_string(cnt)
                + std::string(".shp");
            SaveTmp(fname, *i);

        //считаем центроиды
        TempOMPT centroids(GDALUtilities::CalculateCentroids(*i), 
            destroy);
        if (centroids.get())
        {
            std::string fname = std::string("G:/testmap/test_results/centroids")
                + std::to_string(cnt)
                + std::string(".shp");
            SaveTmp(fname, centroids.get());
        }

        //строим граф смежности
        std::shared_ptr<BridgeGraph>
            graph(CreateGraph(*i,&bm));

        //вычисляем минимальное остовное дерево графа
        std::shared_ptr<MinimumSpanningTree>
            mst(KruskalMST(graph.get()));

        //по минимальному остовному дереву создаем
        //"островки" при помощи convex hull или отрезка+fixed size buffer
        TempOGC bridges(CreateBridgesByTree(mst.get(), *i, &bm), destroy);

        //сохраняем для отладки островки по кластерам
        if (bridges.get())
        {
            std::string fname = std::string("G:/testmap/test_results/bridges")
                + std::to_string(cnt)
                + std::string(".shp");
            SaveTmp(fname, bridges.get());
        }

        for (int i = 0;i < bridges->getNumGeometries();i++)
            final_bridges->addGeometry(bridges->getGeometryRef(i));

        cnt++;
    }

    //выводим статистику создания мостиков
    std::cout << "BridgesRPC statistics:\n";
    std::cout << "\tConvex Hull bridges: " << conv_hull_bridges << std::endl;
    std::cout << "\tBuffered Line bridges: " << buf_line_bridges << std::endl;

#ifdef BRIDGES_UNION
    //объединяем мостики с исходной геометрией
    OGRGeometry *res = bridges->Union(input);
    //возвращаем геометрию
    return dynamic_cast<OGRMultiPolygon*>(res);
#else
    //ускоренный вариант для отладки алгоритма
    //bridges->addGeometry(input);
    //возвращаем геометрию
    return dynamic_cast<OGC*>(final_bridges->clone());
#endif
*/
}

BridgeConstructor::~BridgeConstructor()
{
    //освобождаем память
    OGRGeometryFactory::destroyGeometry(input);
}
