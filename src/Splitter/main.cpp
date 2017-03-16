/*!
\file
\brief Программа упрощения геометрии карт s-57
\details Программа считывает несколько карт s-57.
\details Делит их геометрию регулярной сеткой на тайлы,
которые имеют индекс и группу. Тайлы, принадлеащие к одному
полигону, имеют одинаковую группу.
\details Она может быть использована как отдельный элемент обработки
карт.

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
#include <tiles.h>

OGRErr currentError;

using namespace std;
using namespace GDALUtilities::Boilerplates;

int main(int argc, char *argv[])
{
	//проверяем аргументы командной строки
	if (argc < 3)
    {
        std::cout << "USAGE: Splitter"
            << "<in1> <in2> .. <inN> "
            << "<layer_name> <driver> <outfile>"
            << std::endl;
        std::cout << "<in1>..<inN> - input files" << std::endl;
        std::cout << "<layer_name> - name of layer, from which"
            << "geometries are fetched. It should contain only"
            << "polygons." << std::endl;
        std::cout << "<driver> - name of driver, which you"
            << "prefer to save data with." << std::endl;
        std::cout << "<outfile> - output file name" << std::endl;
		exit(1);
	}

    //имя слоя, из которого импортируем геометрию
    std::string sourceLayerName{argv[argc-3]};
    //название драйвера для файла - результата
    std::string outputDriverName{argv[argc-2]};
    //путь к выходному файлу
    std::string outputFileName{argv[argc-1]};
	//регистрируем все драйверы
	GDALAllRegister();
	//датасеты для каждой из карт s-57
	GDALUtilities::StringList flist;
    for (int i = 1;i < argc - 3;i++)
		flist.push_back(string(argv[i]));

	//набор датасетов
    vector<GDALDataset*> datasets;
    //набор геометрических коллекций из входных файлов
    TempOGC collection(newGeometryCollection(),destroy);

    //подгружаем файлы
    GDALUtilities::LoadDatasets(flist, datasets);
    //из каждого файла читаем нужный слой
    for (vector <GDALDataset*>::iterator i =
        datasets.begin(); i != datasets.end(); i++)
    {
        //обходим dataset'ы, ищем нужный слой
        //Dataset из входного файла S-57
        GDALDataset *inputDataset = *i;
        printf("processing file %s\n", inputDataset->GetFileList()[0]);
        //нужный слой из Dataset входного файла S-57
        OGRLayer *currentLayer =
            inputDataset->GetLayerByName(sourceLayerName.c_str());
        //нет нужного слоя
        if (!currentLayer)
        {
            //выводим сообщение пользователю, что нет слоя sourceLayerName в файле
            //затем пропускаем dataset
            char* fname = inputDataset->GetFileList()[0];
            if (fname)
                printf("file %s does not contain layer %s",
                    fname, sourceLayerName.c_str());
            else
                printf("layer error\n");
            continue;
        }
        //сбрасываем изменения слоя, начинаем читать feature
        currentLayer->ResetReading();
        OGRFeature *currentFeature;
        //красивый прогресс
        GDALUtilities::ProgressIndicator 
            indicator(currentLayer->GetFeatureCount(),
                "Reading file");
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
            //добавляем копию полигон в нашу коллекцию
            collection->addGeometry(currentGeometry);
            OGRFeature::DestroyFeature(currentFeature);
        }
    }

    //теперь генерируем сетку для исходного набора полигонов

    //считаем ее размер
    double grid_sz = BridgesRPC::CalculateGridSize(collection.get());
    //если ошибка расчета сетки, завершаем программу
    if (fabs(grid_sz) < 1E-6)
    {
        std::cout << "Error when calculating grid size"
            << std::endl;
        return 1;
    }

    //генерируем геометрию сетки
    TempOGC grid(GDALUtilities::GenerateGrid(collection.get(), grid_sz),
        destroy);
    //проверяем, что она действительно создалась
    if (!grid.get())
    {
        std::cout << "Error when generating grid geometries"
            << std::endl;
        return 1;
    }

    //делим исходную геометрию сеткой
    //данный алгоритм опирается на свойства файлов shp - 
    //создание атрибутов полигонов
    
    //инициализируем драйвер
    auto shpDriver = static_cast<GDALDriver*>(
        GDALGetDriverByName(outputDriverName.c_str()));
    if (!shpDriver)
    {
        std::cout << "Error when initializing driver \""
            << argv[argc-2] << "\" "
            << std::endl;
        return 1;
    }
    //для драйвера создаем datasource
    GDALDataset *outDataset;
    outDataset = shpDriver->Create(outputFileName.c_str(), 0, 0, 0, GDT_Unknown, NULL);
    if (!outDataset)
    {
        std::cout << "Could not create dataset for output file"
            << std::endl;
        return 1;
    }
    //создаем слой с тайлами
    OGRLayer *outLayer = outDataset->CreateLayer("tiles", NULL, wkbPolygon, NULL);
    if (!outLayer)
    {
        std::cout << "Could not create layer in output file"
            << std::endl;
        return 1;
    }
    //каждый тайл должен содержать два поля - индекс и группу
    OGRFieldDefn oIndexField("index", OFTInteger);
    OGRFieldDefn oGroupField("group", OFTInteger);
    //Инициализируем поля в файле
    if (outLayer->CreateField(&oIndexField) != OGRERR_NONE)
    {
        std::cout << "Could not create index field in output file"
            << std::endl;
        return 1;
    }
    if (outLayer->CreateField(&oGroupField) != OGRERR_NONE)
    {
        std::cout << "Could not create group field in output file"
            << std::endl;
        return 1;
    }
    //запускаем алгоритм разделения по тайлам
    std::shared_ptr<Tiles::TileCollection>
        tiles(BridgesRPC::SplitGeometryByGrid(collection.get(), grid.get()));
    //записываем результат в файл
    for (auto i = tiles->begin();i != tiles->end();i++)
    {
        OGRFeature *poFeature;
        poFeature = OGRFeature::CreateFeature(outLayer->GetLayerDefn());
        //записываем группу и индекс
        poFeature->SetField("index", (*i).second->index());
        poFeature->SetField("group", (*i).second->group());
        //добавляем геометрию в feature
        poFeature->SetGeometry((*i).second->geometry());
        //записываем feature на диск
        if (outLayer->CreateFeature(poFeature) != OGRERR_NONE)
        {
            std::cout << "Failed to create feature"
                << std::endl;
            return 1;
        }
        OGRFeature::DestroyFeature(poFeature);
    }
    //закрываем дескриптор файла
    GDALClose(outDataset);
    //говорим пользователю, что все ок
    std::cout << "Splitting succesful";
    std::cout << std::endl;
	return 0;
}
