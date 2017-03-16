#include "rpcbuffer.h"

//std
#include <memory>
#include <string.h>
#include <vector>
#include <list>
//
#include <gdal.h>
#include <gdal_priv.h>
#include <ogr_feature.h>
#include <ogrsf_frmts.h>
#include <iostream>
#include "KMlocal.h"
//мои библиотеки
#include <gdalutilities.h>
#include <clasterutils.h>

OGRMultiPolygon *BufferRPC::exec(OGRMultiPolygon *input, double buffer_size)
{
    // не принимаем nullptr, это описано в документации
	assert(input != nullptr);
    //текущая ошибка работы GDAL
	OGRErr currentError;

	//разделяем полигоны на большие и маленькие (по площади)
	shared_ptr<ClasterUtils::GeometryClasters> clasterized(
		ClasterUtils::ClasterizeByArea(input, 2));

	//проверяем, нормально ли произошла кластеризация
    //кластеры не должны быть пустыми
	if (clasterized->at(0)->IsEmpty() ||
		clasterized->at(1)->IsEmpty())
	{
		printf("clasterization error!\n");
		return false;
	}

	//берем из кластеров только полигоны
	//clasterized - отсортирован по средней площади
    //то есть clasterized[0] - маленькие полигоны
    //clasterized[1] - большие полигоны
	shared_ptr<OGRMultiPolygon> littleIslands 
        (GDALUtilities::GCtoMP((*clasterized)[0]),OGR_G_DestroyGeometry);
	shared_ptr<OGRMultiPolygon> bigIslands
        (GDALUtilities::GCtoMP((*clasterized)[1]), OGR_G_DestroyGeometry);

	//littleIslands или bigIslands могут не содержать
    //ни одного полигона

    //буферизуем маленькие острова
	shared_ptr<OGRMultiPolygon> buffered(
		static_cast<OGRMultiPolygon*>
		(GDALUtilities::BufferOptimized(littleIslands.get(),buffer_size)),
		OGR_G_DestroyGeometry);

    //функция BufferOptimized не может возвращать nullptr
    assert(buffered != nullptr);

	//теперь объединяем буферные зоны и большие полигоны
	while (!bigIslands->IsEmpty())
	{
        //берем 1-й элемент из коллекции bigIslands
		OGRGeometry *bigIslandGeometry = bigIslands->getGeometryRef(0);
        //в buffered добавляем копию 1-го элемента
		currentError = buffered->addGeometry(
			bigIslandGeometry);
        //удаляем 1-й элемент из коллекции bigIslands
		bigIslands->removeGeometry(0);
		//тип геометрии не может быть неверным
		assert(currentError == OGRERR_NONE);
        //делаем это, пока bigIslands не станет пустым
	}

	//объединяем большие острова и буферные зоны
	shared_ptr<OGRMultiPolygon> res1(
		static_cast<OGRMultiPolygon*>(buffered->UnionCascaded()),
		OGR_G_DestroyGeometry);
    //UnionCascaded может вернуть NULL
    //в случае ошибки
	if (!res1.get())
	{
        //ошибка, возвращаем nullptr
		return nullptr;
	}
    //невалидная геометрия
    if (!res1->IsValid())
    {
        //ошибка
        return nullptr;
    }
	
	//удаляем внешние полости из массива полигонов
	shared_ptr<OGRMultiPolygon> res2(
		GDALUtilities::RemoveRings(res1.get()),
		OGR_G_DestroyGeometry);
	//результат удаления не может быть нулевым
    assert(res2.get() != nullptr);
    //и должен быть валидным
	assert(res2->IsValid());

    //возвращаем копию res2
	return (OGRMultiPolygon*)res2->clone();

}
