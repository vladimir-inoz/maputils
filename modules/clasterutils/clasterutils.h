/*!
\file
\brief Библиотека утилит кластеризации

Библиотека содержит функции для различных видов
кластеризации геометрических коллекций или мультиполигонов.

\author Владимир Иноземцев
\version 1.0
*/

#ifndef CLASTERUTILS_H
#define CLASTERUTILS_H

#include <vector>

class KMlocal;
class KMdata;
class KMfilterCenters;
class OGRGeometryCollection;
class OGRMultiPolygon;

/*!
\brief Различные утилиты кластеризации
\details Алгоритмы кластеризации геометрических коллекций или мультиполигонов
по площади и координатам центроидов
*/
namespace ClasterUtils
{
    /*!
    \brief Кластеры из OGRGeometryCollection
    \details Кластеры, которые могут содержать не только полигоны,
    но и другие типы геометрий. Если нужно преобразовать эти кластеры
    в кластеры полигонов, то исп. функцию GCtoMP
    */
	typedef std::vector<OGRGeometryCollection*> GeometryClasters;

	/*!
    \brief Выводит информацию по работе алгоритма кластеризации
    \param[in] theAlg данные алгоритма KMLocal
    \param[in] dataPts Исходные точки
    \param[in] ctrs Центры кластеризации
    */
	void PrintSummary(
		const KMlocal&		theAlg,	//
		const KMdata&		dataPts,	// the points
		KMfilterCenters&		ctrs); //центры

	/*!
    \brief Средняя площадь полигонов
    \details Считает среднюю площадь полигонов в OGRMultiPolygon
    \param[in] mp Геометрическая коллекция, для которой считается средняя площадь всех геометрий.
    Не допускается nullptr
    \return Средняя площадь полигонов в mp
	*/
	double AverageArea(const OGRGeometryCollection *mp);

	/*!
    * \brief Кластеризация по площадям
    * \details Функция кластеризует полигоны по площадям с использованием
    алгоритма k-means. В возвращаемом векторе полигоны отсортированы по
	средней площади по возрастанию, то есть
	result[0] < result[1] < result[n].
	Возвращает nullptr в случае ошибки
    * \param polygons Мультиполигон, полигоны которого подлежат кластеризации. Не может быть nullptr
    * \param nclasters Число кластеров, на которые нужно разделить полигоны
    \return Набор кластеров
    */
	GeometryClasters *ClasterizeByArea(OGRGeometryCollection * polygons,
		int nclasters = 2);

    /*!
    \brief Кластеризация по координатам центроидов
    \details Функция вычисляет центроиды полигонов, затем формирует кластеры
    методом k-means.
    \param polygons Мультиполигон, полигоны которого подлежат кластеризации. Не может быть nullptr
    \param nclasters Число кластеров, на которые нужно разделить полигоны
    \return Набор кластеров в случае успеха. Nullptr, если polygons - пустая
    геометрия.
    */
	GeometryClasters *ClasterizeByCentroids(OGRGeometryCollection* polygons,
		int nclasters = 10);
}

#endif
