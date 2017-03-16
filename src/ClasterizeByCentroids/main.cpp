/*!
\file
\brief Кластеризация по центроидам
\details Программа предназначена для 
кластеризации входной геометрии по координатам
центроидов. Номер кластера добавляется как
новое поле для геометрии. Кластеризуется
каждый входной файл по отдельности.

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
//кластеризация
#include <KMlocal.h>
//мои модули
#include <gdalutilities.h>
#include <clasterutils.h>

using namespace std;
using namespace GDALUtilities::Boilerplates;

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

void ClasterCore()
{
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
    dataPoints->buildKcTree();
    //создаем структуру центров кластеров
    assert(nclasters > 0);
    centers = new KMfilterCenters(nclasters, *dataPoints);
    //запускаем алгоритм
    KMlocalLloyds kmLloyds(*centers, term);
    *centers = kmLloyds.execute();
    //индексы
    closeCtr = new KMctrIdx[dataPoints->getNPts()];
    //дистанции
    double* sqDist = new double[dataPoints->getNPts()];
    //записываем дистанции
    centers->getAssignments(closeCtr, sqDist);
}

bool HaveClasternum(OGRLayer *currentLayer)
{
    bool res = false;
    OGRFeatureDefn *poFDefn = currentLayer->GetLayerDefn();

    for (int iField = 0; iField < poFDefn->GetFieldCount(); iField++)
    {
        OGRFieldDefn *poFieldDefn = poFDefn->GetFieldDefn(iField);
        if (strcmp(poFieldDefn->GetNameRef(), "clasternum") == 0)
        {
            std::cout << "Have clasternum field" << std::endl;
            res = true;
        }
    }

    if (!res)
        std::cout << "Don't have clasternum field" << std::endl;

    return res;
}


int main(int argc, char *argv[])
{
	//проверяем аргументы командной строки
	if (argc < 4)
	{
        std::cout << "USAGE: ClasterizeByCentroids"
            << "<in1> <in2> .. <inN> "
            << "<layer_name> <nclasters>"
            << std::endl;
        std::cout << "<in1>..<inN> - input files" << std::endl;
        std::cout << "<layer_name> - name of layer, from which"
            << "geometries are fetched. It should contain only"
            << "polygons." << std::endl;
        std::cout << "<nclasters> - number of result clasters,"
            << "must be 2 or greater" 
            << std::endl;
		exit(1);
	}

    //парсим аргументы
    //имя слоя
    string layerName(argv[argc - 2]);
    //число кластеров
    nclasters = atoi(argv[argc - 1]);
    if (nclasters < 2)
    {
        std::cout << "Invalid count of clasters!" << endl;
        std::cout << "Count of clasters should be 2 or greater!"
            << std::endl;
        exit(1);
    }

	//регистрируем все драйверы
	GDALAllRegister();
	//датасеты для каждого из исходных файлов
	GDALUtilities::StringList flist;
	for (int i = 1;i < argc - 2;i++)
		flist.push_back(string(argv[i]));

    //проходимся по списку файлов, пытаемся читать каждый
    for (auto i = flist.begin();i != flist.end();i++)
    {
        //набор геометрических коллекций из данного входного файла
        TempOGC collection(newGeometryCollection(), destroy);

        //Dataset из входного файла
        //Открываем его в режиме чтения
        GDALDataset *inputDataset =
            (GDALDataset*)GDALOpenEx(
                (*i).c_str(), GDAL_OF_VECTOR | GDAL_OF_UPDATE, 
                nullptr, nullptr, nullptr);

        std::cout << "processing file \"" << (*i)
        << "\""  << std::endl;

        //нужный слой из Dataset входного файла
        OGRLayer *currentLayer =
            inputDataset->GetLayerByName(layerName.c_str());
        //нет нужного слоя
        if (!currentLayer)
        {
            //выводим сообщение пользователю, что нет слоя SOURCE_LAYER в файле
            char* fname = inputDataset->GetFileList()[0];
            if (fname)
                cout << "File \"" << fname <<
                "\" does not contain layer \"" <<
                layerName << "\"" << std::endl;
            else
                cout << "layer_error" << std::endl;
            std::cout << "This file contains layers:" << std::endl;
            //выводим список слоев
            for (int i = 0;i < inputDataset->GetLayerCount();i++)
                std::cout << "\"" <<
                inputDataset->GetLayer(i)->GetName()
                << "\" " << std::endl;
            //пропускаем dataset
            continue;
        }
        //проверяем, имеет ли слой нужные нам возможности
        if (!currentLayer->TestCapability(OLCCreateField))
        {
            std::cout << "Layer does not provide OLCCreateField"
                << " capability" << std::endl;
            continue;
        }
        if (!currentLayer->TestCapability(OLCRandomWrite))
        {
            std::cout << "Layer does not provide OLCRandomWrite"
                << " capability" << std::endl;
            continue;
        }
        //проверяем, есть ли поле clasternum
        if (!HaveClasternum(currentLayer))
        {
            //нет, создаем поле clasternum в слое
            OGRFieldDefn oClasterNum("clasternum", OFTInteger);
            if (currentLayer->CreateField(&oClasterNum) != OGRERR_NONE)
            {
                printf("Creating clasternum field failed.\n");
                exit(1);
            }
        }
        //текущая фича
        OGRFeature *currentFeature;

        //смотрим, сколько фич в слое
        int nfeatures = 0;
        currentLayer->ResetReading();
        while ((currentFeature = currentLayer->GetNextFeature()) != nullptr)
        {
            nfeatures++;
            //не забываем удалять, это копия объекта
            OGRFeature::DestroyFeature(currentFeature);
        }

        //для найденного количества фич инициализируем данные KMdata
        dataPoints = new KMdata(2, nfeatures);

        //читаем feature заново, считая центроиды
        currentLayer->ResetReading();
        int featureCounter = 0;
        //красивый прогресс
        GDALUtilities::ProgressIndicator 
            indicator(currentLayer->GetFeatureCount(),
                "Reading file");
        //просматриваем все фичи заново
        while ((currentFeature = currentLayer->GetNextFeature()) != nullptr)
        {
            //отображаем прогресс в консоли
            indicator.incOperationCount();
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
            //считаем failsafe центроид фичи
            shared_ptr<OGRPoint> centroid(
            GDALUtilities::FailsafeCentroid(currentGeometry),
                destroy);
            //перегоняем данные в KMlocal
            KMpoint &p = (*dataPoints)[featureCounter];
            p[0] = centroid->getX();
            p[1] = centroid->getY();

            //освобождаем память фичи
            OGRFeature::DestroyFeature(currentFeature);

            featureCounter++;
        }

        //запускаем алгоритм кластеризации
        ClasterCore();

        //изменяем поле clasternum у каждой фичи
        currentLayer->ResetReading();
        featureCounter = 0;
        //просматриваем все фичи заново
        while ((currentFeature = currentLayer->GetNextFeature()) != nullptr)
        {
            //берем индекс кластера из массива
            int claster_idx = closeCtr[featureCounter++];
            //в фиче меняем поле
            currentFeature->SetField("clasternum", 
                claster_idx);
            //перезаписываем фичу в файле
            currentLayer->SetFeature(currentFeature);
            //освобождаем память фичи
            OGRFeature::DestroyFeature(currentFeature);
        }

        //теперь записываем геометрии кластеров в отдельные файлы
        for (int i = 0;i < nclasters;i++)
        {
            //имя слоя, соответствующего кластеру i
            string lname("claster_");
            lname.append(std::to_string(i));

            std::cout << "processing layer \"" <<
                lname << "\"" << std::endl;

            //инициализация нового слоя
            OGRLayer *newLayer = inputDataset->CreateLayer(lname.c_str(),
                0, wkbPolygon, 0);
            if (!newLayer)
            {
                std::cout << "Error while creating layer"
                    << "\"" << lname << "\"" << std::endl;
                exit(1);
            }

            //в новый слой пробрасываем все атрибуты из исходного слоя
            //для этого сначала инициализируем эти атрибуты
            OGRFeatureDefn *poFDefn = currentLayer->GetLayerDefn();
            for (int iField = 0; iField < poFDefn->GetFieldCount(); iField++)
            {
                OGRFieldDefn *poFieldDefn = poFDefn->GetFieldDefn(iField);
                if (newLayer->CreateField(poFieldDefn) != OGRERR_NONE)
                {
                    std::cout << "Copying field failed."
                        << std::endl;
                    exit(1);
                }
            }

            //текст фильтра для фич
            string filter_str("clasternum=");
            filter_str.append(std::to_string(i));
            //применяем фильтр
            currentLayer->SetAttributeFilter(filter_str.c_str());
            //просматриваем все фичи, соответствующие фильтру
            currentLayer->ResetReading();
            while ((currentFeature = currentLayer->GetNextFeature()) != nullptr)
            {
                //создаем новую фичу в слое кластера
                OGRFeature *poFeature;
                poFeature = OGRFeature::CreateFeature(newLayer->GetLayerDefn());

                //из фичи копируем все атрибуты и геометрию
                if (OGR_F_SetFrom(poFeature, currentFeature, true) != OGRERR_NONE)
                {
                    std::cout << "Copying feature failed."
                        << std::endl;
                }

                //создаем фичу на слое
                if (newLayer->CreateFeature(poFeature) != OGRERR_NONE)
                {
                    std::cout << "Failed to save the feature."
                        << std::endl;
                    exit(1);
                }

                //освобождаем память фичи
                OGRFeature::DestroyFeature(poFeature);
            }
        }
        GDALClose(inputDataset);
    }

    //говорим, что все ок
    std::cout << "Clasterizing ok" << std::endl;
    
	return 0;
}
