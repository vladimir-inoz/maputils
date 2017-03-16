#include "gdalutilities.h"
//std
#include <assert.h>
#include <memory>
#include <algorithm>
#include <iostream>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace GDALUtilities
{

using namespace std;
using namespace Boilerplates;

OGRPoint * Boilerplates::newPoint()
{
    return static_cast<OGRPoint*>
        (OGRGeometryFactory::createGeometry(wkbPoint));
}

OGRPolygon * Boilerplates::newPolygon()
{
    return static_cast<OGRPolygon*>
        (OGRGeometryFactory::createGeometry(wkbPolygon));
}

OGRLinearRing * Boilerplates::newLinearRing()
{
    return static_cast<OGRLinearRing*>
        (OGRGeometryFactory::createGeometry(wkbLinearRing));
}

OGRMultiPoint * Boilerplates::newMultiPoint()
{
    return static_cast<OGRMultiPoint*>
        (OGRGeometryFactory::createGeometry(wkbMultiPoint));
}

OGRMultiPolygon * Boilerplates::newMultiPolygon()
{
    return static_cast<OGRMultiPolygon*>
        (OGRGeometryFactory::createGeometry(wkbMultiPolygon));
}

OGRGeometryCollection * Boilerplates::newGeometryCollection()
{
    return static_cast<OGRGeometryCollection*>
        (OGRGeometryFactory::createGeometry(wkbGeometryCollection));
}

void Boilerplates::destroy(OGRGeometry *geom)
{
    OGRGeometryFactory::destroyGeometry(geom);
}

OGRMultiPolygon * BufferOptimized(OGRMultiPolygon * input, double buf_sz)
{
    //новый мультиполигон с буферами
    CreatePtr(res,MultiPolygon);

    //не принимаем nullptr, это описано в документации
    assert(input != nullptr);
    for (int i = 0;i < input->getNumGeometries();i++)
    {
        //создаем новый полигон путем буферизации
        //i-го полигона из input
        MakePtr(buffered,Polygon,input->getGeometryRef(i)->Buffer(buf_sz, 4));
        //в конец списка геs добавляем новую геометрию
        res->addGeometryDirectly(buffered);
    }

    //число полигонов не должно поменяться!
    assert(input->getNumGeometries() ==
        res->getNumGeometries());

    return res;
}

OGRPolygon* PolygonFromExternalRing(OGRPolygon *input)
{
    // не принимаем nullptr, это описано в документации
    assert(input != nullptr);
    // проверяем, есть ли внутренние контуры полигона
    if (input->getNumInteriorRings() > 0)
    {
        //есть, убираем внутренние контуры
        MakePtr(mls,MultiLineString,ExternalRingToMLS(input));

        if (mls != nullptr)
        {
            assert(mls->IsValid());
            //полигонизируем линию, создается новый объект
            OGRGeometry *polygonized = mls->Polygonize();

            //удаляем mls
            OGRGeometryFactory::destroyGeometry(mls);
            mls = nullptr;

            if (!polygonized)
            {
                //произошла ошибка при полигонизации
                //возвращаем входной полигон
                printf("polygonization error!\n");
                return input;
            }
            else
            {
                if (!polygonized->IsValid())
                {
                    //полигонизация сделана неверно
                    //возвращаем входной полигон
                    printf("polygonization error!\n");
                    return input;
                }
                //возвращаем полигон без внутренних полостей
                OGRPolygon *res = (OGRPolygon*)
                    OGRGeometryFactory::forceToPolygon(polygonized);
                return res;
            }
        }
        //не получилось сделать multilinestring, возвращаем исходный полигон
        return input;
    }
    //нет внутренних контуров, возвращаем исходный полигон
    return input;
}

OGRMultiPolygon *RemoveRings(OGRMultiPolygon* input)
{
    // не принимаем nullptr, это описано в документации
    assert(input != nullptr);

    //новая коллекция, в которую складываем внешние контуры полигонов
    CreatePtr(res,MultiPolygon);

    //не может быть, что мы создали геометрию, и она nullptr
    assert(res != nullptr);

    //убираем из полигонов внутренние контуры
    int n = input->getNumGeometries();
    for (int i = 0;i < n;i++)
    {
        //берем i-й элемент из исходной коллекции
        MakePtr(c,Polygon,input->getGeometryRef(i));
        //в мультиполигоне не может быть не инициализированного элемента
        assert(c != nullptr);
        //получаем полигон из внешней оболочки
        OGRPolygon *newpol = PolygonFromExternalRing(c);
        //он не может быть nullptr
        assert(newpol != nullptr);
        //в новую коллекцию добавляем внешнюю оболочку
        OGRErr error = res->addGeometry(newpol);
        assert(error == OGRERR_NONE);
    }

    //в исходной и новой коллекции не должно быть
    //разное количество элементов
    assert(res->getNumGeometries() ==
        input->getNumGeometries());

    return res;
}

OGRPoint * FailsafeCentroid(OGRGeometry * p)
{
    // не принимаем неинициализированную геометрию
    assert(p != nullptr);

    //создаем новую точку
    CreatePtr(centroid,Point);
    //не должно быть ошибок приведения типов!
    assert(centroid != nullptr);

    //пытаемся рассчитать центроид
    if (p->Centroid(centroid)
        != OGRERR_NONE)
    {
        //не удалось посчитать
        //считаем центр масс BoundingBox полигона
        //берем bounding box полигона
        OGREnvelope bb;
        p->getEnvelope(&bb);
        //считаем координаты центра bounding box
        double x = (bb.MinX + bb.MaxX) / 2;
        double y = (bb.MinY + bb.MaxY) / 2;
        //записываем эти координаты в centroid
        centroid->setX(x);
        centroid->setY(y);
    }

    //проверяем, находится ли центроид за пределами полигона
    if (p->Contains(centroid))
        //внутри полигона, возвращаем центроид
        return centroid;
    else
    {
        //находим ближайшую точку полигона
        auto gtype = p->getGeometryType();
        if (gtype == wkbPolygon)
        {
            auto geom = dynamic_cast<OGRPolygon*>(p);
            MakeSmartPtr(pts,MultiPoint,FetchPointsFromPolygon(geom));
            if (!pts.get())
            {
                //не смогли получить точки полигона
                //ошибка
                return centroid;
            }

            //считаем расстояния между точкой и точками полигона
            //в памяти хранятся пары расстояние-указатель на точку
            vector<pair<double, OGRGeometry*>> distances;
            for (int i = 0;i < pts->getNumGeometries();i++)
            {
                //считаем расстояние
                double dst = centroid->Distance(pts->getGeometryRef(i));
                distances.push_back(pair<double, OGRGeometry*>(dst, pts->getGeometryRef(i)));
            }
            //ищем минимальное расстояние от старого центроида до точек полигона
            //через итератор imin
            auto imin = min_element(distances.begin(), distances.end());
            //разыменовываем итератор и приводим тип к OGRPoint
            auto n_centroid = dynamic_cast<OGRPoint*>((*imin).second->clone());
            //в n_centroid ближайшая точка
            assert(n_centroid);
            return n_centroid;
        }
        else
            return centroid;
    }
}

OGRMultiPoint * CalculateCentroids(OGRGeometryCollection * input)
{
    // не принимаем неинициализированную геометрию
    assert(input != nullptr);

    // создаем новую коллекцию точек
    CreatePtr(centroids,MultiPoint);

    //число элементов input
    int ngeom = input->getNumGeometries();

    //данные прогресса
    ProgressIndicator
        indicator(ngeom, "CalculateCentroids");

    //перебираем элементы input
    for (int i = 0;i < ngeom;i++)
    {
        //обновляем progressbar
        indicator.incOperationCount();
        //текущий полигон
        auto current = input->getGeometryRef(i);
        assert(current);
        //создаем новую точку
        auto centroid =
            FailsafeCentroid(current);
        //добавляем центроид в коллекцию точек
        centroids->addGeometryDirectly(centroid);
    }

    //число точек в коллекции должно быть такое
    //же, как и число полигонов
    assert(centroids->getNumGeometries() ==
        input->getNumGeometries());

    return centroids;
}

OGRMultiPolygon *GCtoMP(OGRGeometryCollection *collection)
{
    //не допускается неинициализированная переменная
    assert(collection != nullptr);

    CreatePtr(mp,MultiPolygon);
    for (int i = 0;i < collection->getNumGeometries();i++)
    {
        OGRGeometry *geom = collection->getGeometryRef(i);
        assert(geom != nullptr);
        if (geom->getGeometryType() == wkbPolygon)
            mp->addGeometryDirectly(geom);
    }
    return mp;
}


OGRMultiPoint * FetchPointsFromMLS(OGRMultiLineString * m)
{
    //не принимаем nullptr
    assert(m != nullptr);

    //инициализируем контейнер для результата функции
    CreatePtr(res,MultiPoint);

    //перебираем коллекцию ломаных
    for (int i = 0;i < m->getNumGeometries();i++)
    {
        //предполагаем, что ext содержит ломаные линии
        //пытаемся взять ломаную линию из коллекции
        OGRGeometry *cur = m->getGeometryRef(i);
        //не бывает неинициализированных геометрий в коллекции
        assert(cur != nullptr);
        //пытаемся преобразовать геометрию к OGRLineString
        MakePtr(ls, LineString, cur);
        //если не получилось, то пропускаем геометрию
        if (!ls) continue;
        //получилось, берем точки из ls
        for (int j = 0;j < ls->getNumPoints();j++)
        {
            //создаем в памяти новую точку
            CreatePtr(newPoint,Point);
            //не должно возникнуть ошибки преобразования типов
            assert(newPoint != nullptr);
            //записываем в точку данные из OGRLineString
            ls->getPoint(j, newPoint);
            //наша точка не может после этого превратиться в nullptr
            assert(newPoint != nullptr);
            //добавляем точку в наш контейнер
            res->addGeometryDirectly(newPoint);
        }
    }

    return res;
}

OGRMultiPoint * FetchPointsFromPolygon(OGRPolygon * p)
{
    //не допускается использовать неинициализированный полигон
    assert(p != nullptr);

    //выделяем внешний контур полигона
    MakeSmartPtr(ext, MultiLineString, ExternalRingToMLS(p));
    if (!ext.get())
    {
        //не удалось преобразовать внешний
        //контур в multilinestring
        return nullptr;
    }

    //берем точки из внешнего контура полигона
    OGRMultiPoint *res = FetchPointsFromMLS(ext.get());
    //res не может быть nullptr, согласно описанию функции
    //FetchPointsFromMLS
    assert(res != nullptr);


    return res;
}

double InscribedCircleRadius(OGRPolygon * p)
{
    assert(p);
    //не принимаем nullptr
    MakeSmartPtr(pts, MultiPoint, FetchPointsFromPolygon(p));
    if (!pts.get())
    {
        //не смогли получить точки полигона
        //ошибка
        return -1;
    }
    if (pts->getNumGeometries() == 0)
        //нет точек, ошибка
        return -1;

    //находим центроид полигона
    OGRPoint* centroid = FailsafeCentroid(p);

    //считаем минимальное расстояние от точек полигона до центроида
    std::vector <double> distances;
    for (int i = 0; i < pts->getNumGeometries(); i++)
    {
        auto point = pts->getGeometryRef(i);
        distances.push_back(point->Distance(centroid));
    }

    //возвращаем минимальное расстояние
    return *std::min_element(std::begin(distances), std::end(distances));
}

GDALDataset *OpenSHPFile(const std::string fname)
{
	//инициализируем драйвер SHP
	GDALDriver *outputDriver =
		GetGDALDriverManager()->GetDriverByName("ESRI Shapefile");
	if (!outputDriver)
	{
        std::cout << "SHP Driver not found" << std::endl;
		return nullptr;
	}
	//открываем файл shp для записи
	string outputFileName(fname.c_str());
    std::cout << "Output filename:" << outputFileName.c_str() << std::endl;
	GDALDataset *outputDataset;
	outputDataset = outputDriver->Create(outputFileName.c_str(),
		0, 0, 0, GDT_Unknown, nullptr);
	if (!outputDataset)
	{
        std::cout << "SHP file could not be created" << std::endl;
		return nullptr;
	}
	return outputDataset;
}

void LoadDatasets(StringList files, std::vector<GDALDataset*> &inputDatasets)
{
    for (auto i : files)
	{
		GDALDataset *dataset =
			(GDALDataset*)GDALOpenEx(
                i.c_str(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr);
		//проверяем, загрузился ли datasource из исходного файла
		if (!dataset)
		{
			//репорт для пользователя
            cout << "Map: " << i << "Error reading datasource" << std::endl;
			continue;
		}
		inputDatasets.push_back(dataset);
        cout << "Map:" << i << "...OK\n" << std::endl;
	}
}

void FreeDatasets(std::vector<GDALDataset*> &inputDatasets)
{
	//освобождаем datasetы после использования
    for (auto it : inputDatasets)
	{
		//проверяем, не может ли быть dataset 0
		//тогда может быть ошибка при его инициализации
        assert(it != nullptr);

        GDALClose(it);
	}
	//очищаем массив
	inputDatasets.clear();
}


void AddPolygonsFromLayer(OGRMultiPolygon* &polygons, OGRLayer *layer)
{
    float prev_progress = 0.0;

	assert(polygons != nullptr);
	assert(layer != nullptr);

	layer->ResetReading();
	OGRFeature *currentFeature;
	int featureCounter = 0;
	while ((currentFeature = layer->GetNextFeature()) != nullptr)
	{

		float progress = (float)(++featureCounter) / (float)layer->GetFeatureCount() * 100.0;
		//printf("\rprocessing feature...%d of %d (%.1f%%)\n", featureCounter,
		//	layer->GetFeatureCount(), progress);
		FancyProgress(progress,prev_progress);
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
		//добавляем полигон в наш набор полигонов
		polygons->addGeometry(currentGeometry);
		OGRFeature::DestroyFeature(currentFeature);
	}
}

OGRMultiPolygon *FetchGeometryFromFiles(StringList files, std::string layerName)
{
	vector<GDALDataset*> datasets;
	OGRMultiPolygon *res = (OGRMultiPolygon*)
		OGRGeometryFactory::createGeometry(wkbMultiPolygon);
	//подгружаем файлы
	LoadDatasets(files, datasets);
	//из каждого файла забираем нужный слой
	for (vector <GDALDataset*>::iterator i =
		datasets.begin(); i != datasets.end(); i++)
	{
		//обходим dataset'ы, ищем нужный слой
		//Dataset из входного файла S-57
		GDALDataset *inputDataset = *i;
		printf("processing file %s\n", inputDataset->GetFileList()[0]);
		//нужный слой из Dataset входного файла S-57
		OGRLayer *currentLayer =
			inputDataset->GetLayerByName(layerName.c_str());
		//нет нужного слоя
		if (!currentLayer)
		{
			//выводим сообщение пользователю, что нет слоя SOURCE_LAYER в файле
			//затем пропускаем dataset
			char* fname = inputDataset->GetFileList()[0];
			if (fname)
				printf("file %s does not contain layer %s",
					fname, layerName.c_str());
			else
				printf("layer error\n");
			continue;
		}
		//есть нужный слой, обходим feature данного dataseta
		AddPolygonsFromLayer(res, currentLayer);
	}

	assert(res != nullptr);

	return res;
}

void FancyProgress(float &progress,float &prev_progress)
{
	static bool first = true;
	if (first)
	{
		printf("[");
		first = false;
	}
	if (progress - prev_progress > 2.0)
	{
		//красивый прогресс
		printf(".");
		prev_progress = progress;
	}
	if (progress == 100.0)
	{
		printf("]\n");
		first = true;
	}
}

OGRMultiLineString* ExternalRingToMLS(OGRPolygon *input)
{
    //не принимаем nullptr, это описано в документации
    assert(input != nullptr);

    //создаем копию внешнего контура полигона input
    OGRLinearRing *ringCopy =
        (OGRLinearRing*)
        input->getExteriorRing()->clone();
    //GDAL не может вернуть nullptr из clone()
    assert(ringCopy != nullptr);
    //преобразуем OGRRing в OGRLineString
    OGRLineString *ls =
        (OGRLineString*)OGRGeometryFactory::
        forceToLineString(ringCopy);
    //из LineString в MultiLineString
    assert(ls != nullptr);
    OGRMultiLineString *mls =
        (OGRMultiLineString*)
        OGRGeometryFactory::forceToMultiLineString(ls);
    //новый объект без ошибок -> возвращаем его
    if (mls->IsValid())
        return mls;
    //новый объект содержит ошибки, удаляем его
    OGRGeometryFactory::destroyGeometry(mls);
    return nullptr;
}

ProgressIndicator::ProgressIndicator(int max_operations,std::string _caption) :
    max_op(max_operations), caption(_caption), progress(0), prev_progress(0), 
    op_cnt(0)
{
}

void ProgressIndicator::incOperationCount()
{
    if (op_cnt == 0)
        std::cout << caption << " progress :" << endl << "[";
    progress = static_cast<float>(op_cnt++) / static_cast<float>(max_op) * 100.0;
    if (progress - prev_progress > 2.0)
    {
        //красивый прогресс
        printf(".");
        prev_progress = progress;
    }
    if (op_cnt == max_op-1) std::cout << "]" << std::endl;
}

OGRPolygon *ConstructPolygon(std::tuple<OGRRawPoint *, int> points)
{
    OGRPolygon *poly;

    OGRLinearRing *rng = Boilerplates::newLinearRing();
    rng->setPoints(std::get<1>(points),std::get<0>(points),nullptr,nullptr);
    rng->closeRings();
    poly = Boilerplates::newPolygon();
    poly->addRingDirectly(rng);

    return poly;
}

double EnvelopeWidth(OGREnvelope &e)
{
   return e.MaxX-e.MinX;
}

double EnvelopeWidth(OGRGeometry *geom)
{
    //Геометрия не должна быть нулевой
    assert(geom);

    OGREnvelope env;
    geom->getEnvelope(&env);
    return EnvelopeWidth(env);
}

double EnvelopeHeight(OGREnvelope &e)
{
    return e.MaxY-e.MinY;
}

double EnvelopeHeight(OGRGeometry *geom)
{
    //Геометрия не должна быть нулевой
    assert(geom);

    OGREnvelope env;
    geom->getEnvelope(&env);
    return EnvelopeHeight(env);
}

bool WriteGeometryCollectionToFile(OGRGeometryCollection *collection, string fileName, string layerName, string driverName)
{
    //инициализируем драйвер
    auto shpDriver = static_cast<GDALDriver*>(
        GDALGetDriverByName(driverName.c_str()));
    if (!shpDriver)
    {
        std::cout << "Error when initializing driver \""
            << driverName.c_str() << "\" "
            << std::endl;
        return false;
    }
    //для драйвера создаем datasource
    auto outDataset = shpDriver->Create(fileName.c_str(), 0, 0, 0, GDT_Unknown, NULL);
    if (!outDataset)
    {
        std::cout << "Could not create dataset for output file"
            << std::endl;
        return false;
    }
    //создаем слой
    OGRLayer *outLayer = outDataset->CreateLayer(layerName.c_str(), NULL, wkbUnknown, NULL);
    if (!outLayer)
    {
        std::cout << "Could not create layer in output file"
            << std::endl;
        //Закрываем dataset
        GDALClose(outDataset);
        return false;
    }
    //Записываем коллекцию
    for (int i=0;i<collection->getNumGeometries();i++)
    {
        //записываем результат в файл
        OGRFeature *poFeature;
        poFeature = OGRFeature::CreateFeature(outLayer->GetLayerDefn());
        //добавляем геометрию в feature
        poFeature->SetGeometry(collection->getGeometryRef(i));
        //записываем feature на диск
        if (outLayer->CreateFeature(poFeature) != OGRERR_NONE)
        {
            std::cout << "Failed to create feature"
                      << std::endl;
            OGRFeature::DestroyFeature(poFeature);
            GDALClose(outDataset);
            return false;
        }
        OGRFeature::DestroyFeature(poFeature);
    }
    //закрываем дескриптор файла
    GDALClose(outDataset);
    return true;
}


OGRGeometryCollection *GenerateTilesInsidePolygon(OGRPolygon *inputPolygon, double gridSize)
{
    using namespace Boilerplates;
    //Контур полигона
    std::shared_ptr<OGRMultiLineString> inputPolygonContour
            (ExternalRingToMLS(inputPolygon));

    //Геометрическая коллекция из одного полигона
    CreateSmartPtr(collection,GeometryCollection);
    collection->addGeometry(inputPolygon);

    //генерируем геометрию сетки
    OGRGeometryCollection *grid = GenerateGrid(collection.get(), gridSize);

    //проверяем, что она действительно создалась
    if (!grid)
    {
        std::cout << "Error when generating grid geometries"
                  << std::endl;

    }

    //исключаем из сетки те квадраты, которые имеют размеры, не кратные grid_sz
    //и те, что не находятся полностью внутри полигона
    int i = 0;
    while (i<grid->getNumGeometries())
    {
        OGRGeometry *current = grid->getGeometryRef(i);
        //Если квадратик внутри полигона, но пересекает его контур, то убираем
        //Если квадратик за пределами полигона, убираем его
        if (!current->Intersects(inputPolygon) || current->Intersects(inputPolygonContour.get()))
        {
            grid->removeGeometry(i);
            continue;
        }
        //Идем к следующему полигону
        i++;
    }

    return grid;
}

std::tuple<int,int> GridSteps(OGRGeometry *input, double gridSize)
{
    int columns = static_cast<int>(ceil(EnvelopeWidth(input) / gridSize));
    int rows = static_cast<int>(ceil(EnvelopeHeight(input) / gridSize));
    return std::make_tuple(rows, columns);
}

OGRPolygon *CreateRectangle(OGRRawPoint &topLeft, double width, double height)
{
    OGRRawPoint *pts = new OGRRawPoint[4];

    pts[0].x = topLeft.x;
    pts[0].y = topLeft.y;

    pts[1].x = topLeft.x + width;
    pts[1].y = topLeft.y;

    pts[2].x = topLeft.x + width;
    pts[2].y = topLeft.y - height;

    pts[3].x = topLeft.x;
    pts[3].y = topLeft.y - height;

    return ConstructPolygon(std::make_tuple(pts,4));
}


OGRPolygon *CreateGridNode(OGRRawPoint &topLeft, double gridSize, int row, int col)
{
    OGRRawPoint newTopLeft(topLeft.x + col*gridSize, topLeft.y - row*gridSize);
    return CreateRectangle(newTopLeft, gridSize, gridSize);
}

OGRGeometryCollection *GenerateGrid(OGRGeometry *input, double gridSize)
{
    assert(input);
    using namespace Boilerplates;
    //создаем сетку
    CreatePtr(grid,GeometryCollection);
    //берем BoundingBox входной карты
    OGREnvelope env;
    input->getEnvelope(&env);
    OGRRawPoint topLeft(env.MinX, env.MaxY);
    //число строк и столбцов сетки
    auto count = GridSteps(input, gridSize);
    int rows = std::get<0>(count),
            cols = std::get<1>(count);
    for (int row = 0; row < rows; row++)
        for (int col = 0; col < cols; col++)
            //Создаем элемент сетки
            grid->addGeometryDirectly(CreateGridNode(topLeft, gridSize, row, col));

    return grid;
}

}
