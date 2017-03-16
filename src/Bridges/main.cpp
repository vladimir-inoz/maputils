/*!
\file
\brief Построение мостиков
\details Программа предназначена для
построения мостиков между полигонами. Для
полигонов рассчитываются центроиды. Для них
строится минимальное остовное дерево. Для ребер
минимального остовного дерева строятся мостики.
\details Если в исходном файле у полигоов есть атрибут
group типа Integer, то полигоны с одинаковым значением
group не будут соединяться мостиками.

\author Владимир Иноземцев
\version 1.0
*/

//gdal
#include <gdal.h>
#include <gdal_priv.h>
#include <ogr_feature.h>
#include <ogrsf_frmts.h>
//std
#include <memory>
#include <string.h>
#include <vector>
#include <list>
#include <iostream>
#include <assert.h>
//мои модули
#include <gdalutilities.h>
#include <rpcbridges_dev.h>

using namespace GDALUtilities::Boilerplates;

//расстояние между точками
double DistanceBetweenPoints(OGRPoint *a,OGRPoint *b)
{
    //проверяем входные данные
    assert(a);
    assert(b);

    //считаем самостоятельно (велосипед)
    double dx = a->getX() - b->getX();
    double dy = a->getY() - b->getY();
    return sqrt(dx*dx + dy*dy);
}

//упорядоченный std::pair, в котором два значения
//сортируются по возрастанию, т.е. first<last
//Используется далее в GroupConnectivityStruct
class SortedPair
{
    //сама пара значений
    //уже отсортирована при конструкторе
    std::pair<int, int> value;
public:

    int first() const {
        return value.first;
    }

    int second() const {
        return value.second;
    }

    SortedPair(int x, int y)
    {
        value = (x < y) ? std::make_pair(x, y) : std::make_pair(y, x);
    }

    bool operator<(const SortedPair& p) const
    {
        return (first() < p.first());
    }
};

//массив полигонов

//структура данных, в которой хранится данные
//о том, какие группы соединены мостиками, и
//сколько мостиков соединяет каждую пару групп
typedef std::map< SortedPair, TempOGC >
GroupConnectivityStruct;

//генерация данной структуры данных по минимальному остовному дереву
GroupConnectivityStruct *GenerateConnectivity(
    BridgesRPC::MinimumSpanningTree *tree,
    Tiles::TileCollection *tiles)
{

    //данные прогресса
    assert(tree);
    GDALUtilities::ProgressIndicator
        indicator(tree->size(), "CreateConnectivity");

    assert(tiles);
    
    //выходная структура данных
    auto conn =
        new GroupConnectivityStruct;

    //перебираем дерево, создаем мостики
    for (auto i = tree->begin();i != tree->end();i++)
    {
        //обновляем прогрессбар
        indicator.incOperationCount();
        //текущее ребро
        BridgesRPC::Edge e = *i;
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
        OGRPolygon *bridge = 
            BridgesRPC::AutoBridge(p_src, p_dst);
        //может возникнуть ошибка при создании мостика
        if (bridge)
        {
            //геометрия должна быть валидна
            assert(bridge->IsValid());

            //пара групп, для которых создан мостик
            SortedPair p(srcTile->group(), destTile->group());

            //ищем, соединена ли уже пара групп
            if (conn->find(p) != conn->end())
            {
                //уже соединена, добавляем новый мостик
                //в коллекцию геометрий
                conn->at(p)->addGeometryDirectly(bridge);
            }
            else
            {
                //еще не соединена, создаем новую коллекцию геометрий
                //мостиков
                TempOGC gc(newGeometryCollection(), destroy);
                gc->addGeometryDirectly(bridge);
                conn->insert(
                    std::make_pair(p,gc)
                    );
            }

        }
        else
        {
            //не удалось создать мостик
            //говорим об этом пользователю
            std::cout << "Error when creating bridge"
                << std::endl;
        }
    }

    //Возвращаем структуру данных
    return conn;
}

//оптимизация структуры данных - для каждой пары групп полигонов
//оставляется только один мостик с минимальной площадью
void OptimizeConnectivity(GroupConnectivityStruct *conn)
{
    //перебираем структуру по ключам
    for (auto i = conn->begin();i != conn->end();i++)
    {
        //создаем массив площадей мостиков
        //каждый элемент - пара <площадь, индекс полигона>
        typedef std::pair <double, int> AreaPair;
        std::vector< AreaPair > areas;

        //текущая коллекция геометрий
        auto currentCollection = (*i).second;

        //перебираем все элементы структуры, считаем площади
        //заносим в areas
        for (int j = 0;j < currentCollection->getNumGeometries();j++)
        {
            //текущая геомерия
            auto currentGeometry = currentCollection->
                getGeometryRef(j);
            //приводим ее к полигону
            auto poly = static_cast<OGRPolygon*>(currentGeometry);
            assert(poly);
            //у полигона считаем площадь
            double area = poly->get_Area();
            //assert(area > 0);
            //создаем пару значений <площадь, индекс полигона>
            auto np =
                 std::make_pair(area, j);
            //добавляем в areas
            areas.push_back(np);
        }

        //сортируем areas по возрастанию площади
        std::sort(areas.begin(), areas.end(),
            [](AreaPair a, AreaPair b) {return a.first < b.first;});

        //берем из исходной коллекции геометрий
        //мостик с минимальной площадью
        //копируем его
        assert(areas.size() > 0);
        OGRGeometry *minimumAreaBridge =
            currentCollection->getGeometryRef(areas[0].second)->
            clone();

        //очищаем исходную коллекцию геометрий
        currentCollection->empty();

        //заново добавляем в коллекцию мостик с минимальной
        //площадью
        currentCollection->addGeometryDirectly(minimumAreaBridge);
    }
}


int main(int argc, char *argv[])
{
    //проверяем аргументы командной строки
    if (argc < 5)
    {
        std::cout << "USAGE: Bridges"
            << "<in1> .. <inN> "
            << "<layer_name> "
            << "<driver> "
            << "<outfile>"
            << std::endl;
        std::cout << "<in1>..<inN> - input files" << std::endl;
        std::cout << "<layer_name> - name of layer, from which "
            << "geometries are fetched." << std::endl
            << " It should contain only polygons."
            << std::endl;
        std::cout << "<driver> - name of driver, which you "
            << "prefer to save data with." << std::endl;
        std::cout << "<outfile> - output file name" << std::endl;
        exit(1);
    }
    //парсим аргументы
    //имя слоя
    std::string layerName(argv[argc - 3]);
    //имя драйвера
    std::string driverName(argv[argc - 2]);
    //имя выходного файла
    std::string outputFileName(argv[argc - 1]);

    //регистрируем все драйверы
    GDALAllRegister();
    //список входных файлов
    GDALUtilities::StringList flist;
    for (int i = 1;i < argc - 3;i++)
        flist.push_back(std::string(argv[i]));

    //набор тайлов из входных файлов
    std::shared_ptr<Tiles::TileCollection> tiles
        = std::make_shared<Tiles::TileCollection>();

    //проходимся по списку файлов, пытаемся читать каждый
    for (auto i = flist.begin();i != flist.end();i++)
    {

        //Dataset из входного файла
        //Открываем его в режиме только чтения
        GDALDataset *inputDataset =
            (GDALDataset*)GDALOpenEx(
                (*i).c_str(), GDAL_OF_VECTOR,
                nullptr, nullptr, nullptr);

        std::cout << "processing file \"" << (*i)
            << "\"" << std::endl;

        //нужный слой из Dataset входного файла
        OGRLayer *currentLayer =
            inputDataset->GetLayerByName(layerName.c_str());

        //нет нужного слоя
        if (!currentLayer)
        {
            //выводим сообщение пользователю, что нет слоя SOURCE_LAYER в файле
            char* fname = inputDataset->GetFileList()[0];
            if (fname)
                std::cout << "File \"" << fname <<
                "\" does not contain layer \"" <<
                layerName << "\"" << std::endl;
            else
                std::cout << "layer_error" << std::endl;
            std::cout << "This file contains layers:" << std::endl;
            //выводим список слоев
            for (int i = 0;i < inputDataset->GetLayerCount();i++)
                std::cout << "\"" <<
                inputDataset->GetLayer(i)->GetName()
                << "\" " << std::endl;
            //пропускаем dataset
            continue;
        }

        //проверяем, что в слое только полигоны
        if (currentLayer->GetGeomType() != wkbPolygon)
        {
            //другой тип геометрии
            std::cout << "Layer \"" << currentLayer->GetName()
                << "\" has incorrect geometry type";
            //пропускаем dataset
            continue;
        }

        //проверяем, есть ли в слое поле "group"
        OGRFeatureDefn *poFDefn = currentLayer->GetLayerDefn();
        bool haveGroups = false;
        for (int iField = 0; iField < poFDefn->GetFieldCount(); iField++)
        {
            OGRFieldDefn *poFieldDefn = poFDefn->GetFieldDefn(iField);
            if (strcmp(poFieldDefn->GetNameRef(), "group") == 0)
                haveGroups = true;
        }

        //просматриваем все фичи слоя, считываем полигоны
        int featureCounter = 0;
        currentLayer->ResetReading();
        OGRFeature *currentFeature;
        while ((currentFeature = currentLayer->GetNextFeature()) != nullptr)
        {
            //геометрия из входного файла
            OGRGeometry *currentGeometry;
            currentGeometry = currentFeature->GetGeometryRef();
            //если нет геометрии в currentFeature, пропускаем его
            if (!currentGeometry)
            {
                OGRFeature::DestroyFeature(currentFeature);
                continue;
            }
            //если геометрия не того типа, пропускаем feature
            if (currentGeometry->getGeometryType() != wkbPolygon)
            {
                OGRFeature::DestroyFeature(currentFeature);
                continue;
            }

            int group = 0;
            //если есть поле group, то читаем его
            if (haveGroups)
                group = currentFeature->GetFieldAsInteger("group");

            //добавляем полигон как тайл в коллекцию
            Tiles::Tile *t = new Tiles::Tile(
                currentGeometry->clone(), group);
            tiles->addTile(t);

            //освобождаем память фичи
            OGRFeature::DestroyFeature(currentFeature);
            featureCounter++;
        }
        //закрываем файл
        GDALClose(inputDataset);
    }

    //граф смежности
    std::shared_ptr<BridgesRPC::BridgeGraph> graph(
        BridgesRPC::CreateGraph(tiles.get(), 1.0));

    //считаем минимальное остовное дерево для графа
    std::shared_ptr<BridgesRPC::MinimumSpanningTree> tree
        (BridgesRPC::KruskalMST(graph.get()));

    //создаем структуру соединения пар групп мостиками
    //по минимальному остовному дереву
    std::shared_ptr<GroupConnectivityStruct> conn
        (GenerateConnectivity(tree.get(), tiles.get()));

    //оставляем только по одному мостику, соединяющему
    //каждую пару групп, причем с наименьшей площадью
    OptimizeConnectivity(conn.get());

    //сохраняем их в отдельный файл
    GDALDriver *outDriver;
    outDriver = GetGDALDriverManager()->
        GetDriverByName(driverName.c_str());
    if (outDriver == NULL)
    {
        std::cout << "driver " << driverName <<
            " is not avaliable" << std::endl;
        exit(1);
    }

    //создаем dataset
    GDALDataset *outDataset;
    outDataset = outDriver->Create(outputFileName.c_str(),
        0, 0, 0, GDT_Unknown, NULL);
    if (!outDataset)
    {
        std::cout << "Creation of output file failed."
            << std::endl;
        exit(1);
    }

    //создаем слой в выходном файле
    OGRLayer *outLayer;
    outLayer = outDataset->CreateLayer("bridges", NULL,
        wkbPolygon, NULL);
    if (outLayer == NULL)
    {
        std::cout << "Layer creation failed."
            << std::endl;
        exit(1);
    }

    //в слой записываем фичи с мостиками
    for (auto i = conn->begin();i != conn->end();i++)
    {
        OGC *currentCollection = (*i).second.get();
        for (int j = 0;j < currentCollection->getNumGeometries();j++)
        {
            OGRFeature *poFeature;
            poFeature = OGRFeature::CreateFeature(outLayer->GetLayerDefn());
            poFeature->SetGeometry(currentCollection->getGeometryRef(j));
            if (outLayer->CreateFeature(poFeature) != OGRERR_NONE)
            {
                std::cout << "Failed to create feature in shapefile."
                    << std::endl;
                exit(1);
            }
            OGRFeature::DestroyFeature(poFeature);
        }
    }

    //Закрываем файл
    GDALClose(outDataset);

	return 0;
}
