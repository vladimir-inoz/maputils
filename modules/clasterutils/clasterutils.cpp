#include "clasterutils.h"

//std
#include <iostream>
#include <vector>
#include <algorithm>
//gdal
#include <gdal.h>
#include <gdal_priv.h>
#include <ogr_feature.h>
#include <ogrsf_frmts.h>
//km
#include <KMlocal.h>
#include <gdalutilities.h>

namespace ClasterUtils
{
    using namespace GDALUtilities::Boilerplates;
	/*!
    \brief Структура данных кластеризации
    \details Используется во внутренних функциях модуля
    ClasterUtils
    */
	struct ClasterData
	{
		///непосредственно данные
		KMdata *dataPoints;
		///число кластеров
		int nclasters;
		///центры кластеризации
		KMfilterCenters *centers;
		///исходные геометрии
		OGRGeometryCollection *src;
		///массив индексов
		KMctrIdxArray closeCtr;
	};
	///инициазация ClasterData
	void InitClasterData(ClasterData &data, int nclasters);
	///уничтожение ClasterData
	void DestroyClasterData(ClasterData &data);
	/*!
    \brief Создание данных кластеризации из площадей полигонов
    \param[in] polygons Исходный мультиполигон. Не допускается nullptr
    \param[out] cdata Структура данных кластеризации
	\return Возвращает true в случае успеха. Если число полигонов в
    polygons нулевое, или произошла ошибка создания данных, то возвращает
    false.
	*/
	bool CreateClasterDataWithAreas(ClasterData& cdata, 
		OGC *polygons);
    /*!
    \brief Создание данных кластеризации из центроидов полигонов
    \param[in] polygons Исходный мультиполигон. Не допускается nullptr
    \param[out] cdata Структура данных кластеризации
    \return Возвращает true в случае успеха. Если число полигонов в
    polygons нулевое, или произошла ошибка создания данных, то возвращает
    false.
    */
	bool CreateClasterDataWithCentroids(ClasterData& cdata, 
		OGC *polygons);
	/*!
    \brief Основной алгоритм модуля ClasterUtils
    \details Функция выполняет алгоритм кластеризации данных
    методом k-means с использованием библиотеки KMLocal
    \param cdata Данные кластеризации, для которых выполняется алгоритм
    */
	void ClasterCore(ClasterData &cdata);
	/*!
    \brief Сортировка полигонов на основании результатов кластеризации
    \param[in] cdata Данные кластеризации
    \return Кластеры полигонов, созданные из данных cdata
    */
	GeometryClasters *SortGeometry(ClasterData &cdata);
}

void ClasterUtils::PrintSummary(
	const KMlocal&		theAlg,		// the algorithm
	const KMdata&		dataPts,	// the points
	KMfilterCenters&		ctrs)		// the centers
{
	using std::cout;
	cout << "Number of stages: " << theAlg.getTotalStages() << "\n";
	cout << "Average distortion: " <<
		ctrs.getDist(false) / double(ctrs.getNPts()) << "\n";
	// print final center points
	cout << "(Final Center Points:\n";
	ctrs.print();
	cout << ")\n";
	// get/print final cluster assignments
	KMctrIdxArray closeCtr = new KMctrIdx[dataPts.getNPts()];
	double* sqDist = new double[dataPts.getNPts()];
	ctrs.getAssignments(closeCtr, sqDist);

	*kmOut << "(Cluster assignments:\n"
		<< "    Point  Center  Squared Dist\n"
		<< "    -----  ------  ------------\n";
	for (int i = 0; i < dataPts.getNPts(); i++) {
		*kmOut << "   " << setw(5) << i
			<< "   " << setw(5) << closeCtr[i]
			<< "   " << setw(10) << sqDist[i]
			<< "\n";
	}
	*kmOut << ")\n";
	delete[] closeCtr;
	delete[] sqDist;
}

double ClasterUtils::AverageArea(const OGRGeometryCollection *mp)
{
	assert(mp != nullptr);
	//если пустой контейнер, то не считаем площадь
	if (mp->IsEmpty())
		return 0.0;
	double res = 0.0;
	for (int i = 0;i < mp->getNumGeometries();i++)
	{
		OGRPolygon *geom =
			(OGRPolygon*)mp->getGeometryRef(i);
		assert(geom != nullptr);
		res += geom->get_Area();
	}
	res /= mp->getNumGeometries();
	return res;
}

void ClasterUtils::ClasterCore(ClasterUtils::ClasterData &cdata)
{
	assert(cdata.dataPoints != nullptr);

	int stages = 100;
	KMterm	term(100, 0, 0, 0,		// run for 100 stages
		0.10,			// min consec RDL
		0.10,			// min accum RDL
		3,			// max run stages
		0.50,			// init. prob. of acceptance
		10,			// temp. run length
		0.95);			// temp. reduction factor
						//массив данных для алгоритма

	//строим дерево для сортировки
	cdata.dataPoints->buildKcTree();
	//создаем структуру центров кластеров
	assert(cdata.nclasters > 0);
	cdata.centers = new KMfilterCenters(cdata.nclasters, *cdata.dataPoints);
	//запускаем алгоритм
	KMlocalLloyds kmLloyds(*cdata.centers, term);	
	*cdata.centers = kmLloyds.execute();
	//индексы
	cdata.closeCtr = new KMctrIdx[cdata.dataPoints->getNPts()];
	//дистанции
	double* sqDist = new double[cdata.dataPoints->getNPts()];
	//записываем дистанции
	cdata.centers->getAssignments(cdata.closeCtr, sqDist);
}

void ClasterUtils::InitClasterData(ClasterData & data,int nclasters)
{
	data.dataPoints = nullptr;
	data.nclasters = nclasters;
	data.src = nullptr;
	data.centers = nullptr;
	data.closeCtr = nullptr;
}

void ClasterUtils::DestroyClasterData(ClasterData & data)
{
	//уничтожаем данные кластеризации
	delete data.dataPoints;
	data.dataPoints = nullptr;
	//центры
	delete data.centers;
	data.centers = nullptr;
	//индексы
	delete data.closeCtr;
	data.closeCtr = nullptr;
}

bool ClasterUtils::CreateClasterDataWithAreas(ClasterData& cdata,OGRGeometryCollection *polygons)
{
	const int dimension_points = 1;
	assert(polygons != nullptr);

	//число геометрий
	int ngeom = polygons->getNumGeometries();
	if (ngeom < 1)
	{
		//если нет геометрий, то не будем инициализировать
		cdata.dataPoints = nullptr;
		return false;
	}
	//новый объект с данными кластеризации
	cdata.dataPoints = new KMdata(dimension_points, ngeom);
	//формируем массив из площадей полигонов
	for (int i = 0;i < polygons->getNumGeometries();i++)
	{
		KMpoint &p = (*cdata.dataPoints)[i];
		OGRPolygon *g = 
			(OGRPolygon*)polygons->getGeometryRef(i);
		assert(g != nullptr);
		double area = g->get_Area();
		//не бывает полигонов с нулевой площадью!
		assert(area > 0);
		p[0] = area;
	}
	//число точек
	cdata.dataPoints->setNPts(ngeom);
	cdata.src = polygons;
	return true;
}

bool ClasterUtils::
CreateClasterDataWithCentroids(ClasterData & cdata, OGRGeometryCollection * polygons)
{
	const int dimension_points = 2;
	assert(polygons != nullptr);

	//число геометрий
	int ngeom = polygons->getNumGeometries();
	if (ngeom < 1)
	{
		//если нет геометрий, то не будем инициализировать
		cdata.dataPoints = nullptr;
		return false;
	}
	//новый объект с данными кластеризации
	cdata.dataPoints = new KMdata(dimension_points, ngeom);
	//формируем массив из центроидов полигонов
	for (int i = 0;i < polygons->getNumGeometries();i++)
	{
		KMpoint &p = (*cdata.dataPoints)[i];
		OGRPolygon *g =
			(OGRPolygon*)polygons->getGeometryRef(i);
		assert(g != nullptr);
		//создаем новый центроид
		OGRPoint *centroid;
		centroid = GDALUtilities::FailsafeCentroid(g);
		assert(centroid != nullptr);
		p[0] = centroid->getX();
		p[1] = centroid->getY();
		//удаляем центроид
		OGRGeometryFactory::destroyGeometry(centroid);
	}
	//число точек
	cdata.dataPoints->setNPts(ngeom);
	cdata.src = polygons;
	return true;
}

ClasterUtils::GeometryClasters *
ClasterUtils::SortGeometry(ClasterData &cdata)
{
	assert(cdata.nclasters > 0);
	assert(cdata.centers != nullptr);
	assert(cdata.closeCtr != nullptr);
	assert(cdata.dataPoints != nullptr);
	assert(cdata.src != nullptr);

	GeometryClasters *result;
	result = new GeometryClasters;
	//инициализируем списки принадлежности к кластерам
	result->resize(cdata.nclasters);
	//инициализируем новые контейнеры геометрии
	for (int i = 0;i < cdata.nclasters;i++)
		(*result)[i] = (OGC*)
		OGRGeometryFactory::createGeometry(wkbGeometryCollection);

	for (int i = 0;i < cdata.nclasters;i++)
		assert((*result)[i] != nullptr);

	for (int i = 0;i < cdata.dataPoints->getNPts();i++)
	{
		//номер списка, в который добавлять текущий полигон
		KMctrIdx list_index = cdata.closeCtr[i];
		assert(list_index >= 0 && list_index < cdata.nclasters);
		//добавляем копию полигона
		(*result)[list_index]->addGeometry(
			cdata.src->getGeometryRef(i));
	}
	//печатаем результат работы
	for (int i = 0;i < cdata.nclasters;i++)
	{
		printf("claster %d npolygons = %d\n", i, (*result)[i]->getNumGeometries());
	}

	return result;
}

ClasterUtils::GeometryClasters 
*ClasterUtils::ClasterizeByArea(OGRGeometryCollection * polygons,int nclasters)
{
	ClasterData cdata;
	InitClasterData(cdata,nclasters);
	//заполняем данными о площади полигонов
	if (!CreateClasterDataWithAreas(cdata, polygons))
	{
		printf("could not create claster data\n");
		return nullptr;
	}
	//запускаем алгоритм кластеризации
	ClasterCore(cdata);
	GeometryClasters *res;
	//сортируем геометрию по кластерам
	res = SortGeometry(cdata);
	//сортируем кластеры по средней площади
	std::sort(res->begin(), res->end(),
		[](OGC* const &a, OGC* const &b) -> bool
	{return AverageArea(a) < AverageArea(b);});
	//выводим информацию о средних площадях полигонов
	printf("Clasters average area:\n");
	int claster_cnt = 0;
	for_each(res->begin(), res->end(),
		[&](OGC* const &a) mutable
	{printf("Claster %d: %f\n",claster_cnt++, AverageArea(a));});
	return res;
}

ClasterUtils::GeometryClasters* 
ClasterUtils::ClasterizeByCentroids(OGRGeometryCollection * polygons, int nclasters)
{
	ClasterData cdata;
    //проверяем, что геометрия не пустая
    if (polygons->getNumGeometries() == 0)
        return nullptr;
    //сравниваем число точек и число кластеров
    //если число точек меньше числа кластеров
    //то число кластеров = число точек
    if (polygons->getNumGeometries() < nclasters)
        nclasters = polygons->getNumGeometries();
	InitClasterData(cdata, nclasters);
	//заполняем данными о центроидах
	if (!CreateClasterDataWithCentroids(cdata, polygons))
	{
		printf("could not create claster data\n");
		return nullptr;
	}
	//запускаем алгоритм кластеризации
	ClasterCore(cdata);
	GeometryClasters *res;
	//сортируем геометрию по кластерам
	res = SortGeometry(cdata);
	return res;
}