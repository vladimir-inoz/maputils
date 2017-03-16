/*!
\file
\brief Полезные утилиты для работы с GDAL

\author Владимир Иноземцев
\version 1.3
*/

#ifndef GDAL_UTILITIES_H
#define GDAL_UTILITIES_H

#include <gdal.h>
#include <gdal_priv.h>
#include <ogr_feature.h>
#include <ogrsf_frmts.h>
#include <string.h>
#include <vector>
#include <list>
#include <memory>
#include <tuple>

namespace GDALUtilities
{
    /*!
    \defgroup gdalutilities Утилиты GDAL
    \brief Утилиты GDAL
    \details Различные полезные функции, связанные
    с преобразованиями геометрии OGR и вычислениями,
    которые используются в проекте.
    @{
    */

    ///alias для предотвращения boilerplate
    typedef std::vector<std::string> StringList;
    ///alias для предотвращения boilerplate
    typedef StringList::iterator StringListIterator;

    namespace Boilerplates
    {
        ///boilerbpate типов OGR
        typedef OGRGeometryCollection OGC;
        typedef OGRMultiPolygon OMP;
        typedef OGRMultiPoint OMPT;
        typedef OGRPoint OP;
        typedef OGRLinearRing OLR;

        //boilerplate shared_ptr
        typedef std::shared_ptr<OGC> TempOGC;
        typedef std::shared_ptr<OMP> TempOMP;
        typedef std::shared_ptr<OMPT> TempOMPT;
        typedef std::shared_ptr<OLR> TempOLR;

        //boilerplate createGeometry
        OGRPoint *newPoint();
        OGRPolygon *newPolygon();
        OGRLinearRing *newLinearRing();
        OGRMultiPoint *newMultiPoint();
        OGRMultiPolygon *newMultiPolygon();
        OGRGeometryCollection *newGeometryCollection();

        //boilerplate destroyGeometry
        void destroy(OGRGeometry* geom);

        //Структура для хранения данных о тайлах
        typedef std::vector<std::vector<OGRPolygon*>> TileMap;
        typedef std::shared_ptr<TileMap> TileMapPtr;

        /*!
          * Макрос для инициализации умного указателя на пустую геометрию
          * нужного типа
          * name Имя указателя
          * type Тип геометрии
          * Например CreateSmartPtr(point,wkbPoint)
          */
#define CreateSmartPtr(name,type) std::shared_ptr<OGR##type> name \
    (static_cast<OGR##type*>(OGRGeometryFactory::createGeometry(wkb##type)), \
    OGRGeometryFactory::destroyGeometry);

        /*!
          * Макрос для инициализации указателя на пустую геометрию
          * нужного типа
          * name Имя указателя
          * type Тип геометрии
          * Например CreateSmartPtr(point,wkbPoint)
          */
#define CreatePtr(name,type) OGR##type *name = \
        static_cast<OGR##type*>(OGRGeometryFactory::createGeometry(wkb##type));


        /*!
          * Макрос для инициализации умного указателя результатом выражения
          * нужного типа
          * name Имя указателя
          * type Тип геометрии
          * expr Выражение инициализации
          * Например MakeSmartPtr(cloned,GeometryCollection,res->clone())
          */
#define MakeSmartPtr(name,type,expr) std::shared_ptr<OGR##type> name \
    (static_cast<OGR##type*>(expr), OGRGeometryFactory::destroyGeometry);

        /*!
          * Макрос для инициализации указателя результатом выражения
          * нужного типа
          * name Имя указателя
          * type Тип геометрии
          * expr Выражение инициализации
          * Например MakePtr(cloned,GeometryCollection,res->clone())
          */
#define MakePtr(name,type,expr) OGR##type *name = static_cast<OGR##type*>(expr);

    }
    /*!
    \defgroup gdu_tiles Генерация тайлов по различным алгоритмам
    \ingroup gdalutilities
    \brief Генерация тайлов
    @{
    */

    ///Расчет числа шагов сетки для покрытия заданной геометрии
    std::tuple<int,int> GridSteps(OGRGeometry *input, double gridSize);

    ///Создание полигона-прямоугольника
    OGRPolygon *CreateRectangle(OGRRawPoint &topLeft, double width, double height);

    ///Создание элемента сетки
    OGRPolygon *CreateGridNode(OGRRawPoint &topLeft, double gridSize, int row, int col);

    /*!
    \brief Генерация сетки.
    \details Создается сетка с регулярным шагом, которая
    покрывает исходную геометрию
    \param[in] grid_sz шаг сетки
    \param[in] fancyProgress Если True, то в консоли рисуется красивый прогрессбар
    \param[in] debugFile Если True, то сетка записывается в отладочный файл shp
    \return Новая геометрия, содержащая полигоны - квадраты
    регулярной сетки.
    */
    OGRGeometryCollection *GenerateGrid(OGRGeometry *input,
        double gridSize = 0.01);

    /*!
     * \brief Генерация квадратных тайлов, которые находятся целиком внутри полигона и не пересекают его контур
     * \param[in] inputPolygon Исходный полигон
     * \param[in] gridSize Размер сетки тайлов
     * \return Коллекция тайлов
     */
    OGRGeometryCollection *GenerateTilesInsidePolygon(OGRPolygon *inputPolygon, double gridSize);

    /*!
      @}
      */

    /*!
    \defgroup gdu_conversions Преобразования геометрии
    \ingroup gdalutilities
    \brief Преобразования геометрии
    @{
    */

    /*!
    \brief Буферизация мультиполигона
    \details Оптимизированная по количеству используемой памяти
    Функция буферизации мультиполигона
    \param[in] input Исходный мультиполигон. Не допускается nullptr.
    Допускается пустой мультиполигон (getGeometryNum()==0)
    \param[in] buf_sz Размер буфера в единицах карты
    \return Новый буферизованный мультиполигон, или nullptr в случае ошибки
    */
    OGRMultiPolygon *BufferOptimized(OGRMultiPolygon *input, double buf_sz);

    /*!
    \brief Создание нового полигона из внешней оболочки исходного
    \details Создает новый полигон из внешней оболочки полигона input
    Памятью управляет вызывающая функция.
    \param[in] input Исходный полигон. Не допускается nullptr
    \return Новый полигон в случае успеха, исходный полигон в случае ошибки
    */
    OGRPolygon* PolygonFromExternalRing(OGRPolygon *input);

    /*!
    \brief Удаление полостей из полигонов в коллекции
    \details Удаляет внутренние полости из массива полигонов,
    если они там есть.
    \param[in] input Исходный мультиполигон. Не допускается nullptr
    \return Новый мультиполигон. Количество полигонов в нем
    совпадает с числом полигонов в исходной коллекции.
    */
    OGRMultiPolygon *RemoveRings(OGRMultiPolygon *input);

    /*!
    \brief Отказоустойчивый расчет центроида
    \details Расчет центроида полигона. Если не
    удалось рассчитать центроид методом Centroid(),
    то центроид считается как центр BoundingBox
    полигона. Если центроид находится за пределами
    полигона, то он "притягивается" к ближайшей точке
    полигона.
    \param[in] p Полигон, для которого считается
    центроид. Не допускается nullptr.
    \return Новый OGRPoint с центроидом. Не возвращается
    nullptr.
    */
    OGRPoint *FailsafeCentroid(OGRGeometry* p);

    /*!
    \brief Создает коллекцию центроидов.
    \details Центроиды соответствуют коллекции
    полигонов input по порядку. Если не удалось
    рассчитать центроид, то вычисляется центр
    BoundingBox полигона.
    \param[in] input Исходная коллекция геометрий.
    Не допускается nullptr. Может быть пустой.
    \return Новая коллекция точек - центроидов.
    Число точек равно числу полигонов.
    */
    OGRMultiPoint *CalculateCentroids(OGRGeometryCollection *input);

    /*!
    \brief Преобразование GeometryCollection в MultiPolygon
    \details Если в collection нет ни одного полигона
    типа wkbPolygon, то новый полигон будет пустым.
    \param[in] collection Исходная коллекция геометрий.
    Не допускается nullptr
    \return Новый мультиполигон.
    Не может быть nullptr. Может быть пустым (т.е. не
    содержать объектов)
    Он содержит ссылки на полигоны из
    GeometryCollection, поэтому при удалении collection
    нельзя будет обращаться к полигонам из MultiPolygon
    */
    OGRMultiPolygon *GCtoMP(OGRGeometryCollection *collection);

    /*!
    \brief Преобразование внешнего контура полигона в MultiLineString
    \details Пытается создать новый объект MultiLineString из
    внешней оболочки полигона. Если полученный объект
    содержит ошибки, то возвращается nullptr.
    \param[in] input Исходный полигон. Не допускается nullptr
    \return Новый MultiLineString, или nullptr в случае ошибки
    */
    OGRMultiLineString* ExternalRingToMLS(OGRPolygon *input);

    /*!
    \brief Массив точек MultiLineString
    \details Создается коллекция точек, входящих
    в состав коллекции ломаных линий.
    \param[in] m Исходная ломаная линия. Не допускается nullptr.
    Может быть пустой
    \result Новая коллекция точек из MultiLineString. Не может
    быть nullptr. Может быть пустой.
    \todo протестировать функцию
    */
    OGRMultiPoint *FetchPointsFromMLS(OGRMultiLineString *m);

    /*!
    \brief Массив точек полигона
    \details Создается коллекция точек, входящих
    в состав внешнего контура полигона.
    \param[in] p Исходный полигон. Не допускается nullptr.
    \return Новая коллекция точек полигона, nullptr в 
    случае ошибки.
    \todo протестировать функцию
    */
    OGRMultiPoint *FetchPointsFromPolygon(OGRPolygon *p);

    /*!
    \brief Радиус вписанной в полигон окружности
    \details Радиус рассчитывается как расстояние от 
    центроида полигона до ближайшей к нему точки.
    \param[in] p Исходный полигон. Не допускается nullptr.
    \return Радиус вписанной окружности в единицах карты,
    или -1 в случае ошибки.
    */
    double InscribedCircleRadius(OGRPolygon *p);

    /*!
     * \brief ConstructPolygon
     * \param points Массив точек <x,y>
     * \return Новый полигон
     */
    OGRPolygon *ConstructPolygon(std::tuple<OGRRawPoint*,int> points);

    /*!
     * \brief Расчет ширины OGREnvelope
     * \param[in] e Исходный bounding box
     * \return Ширина Bounding Box
     */
    double EnvelopeWidth(OGREnvelope &e);

    /*!
     * \brief Расчет ширины геометрии с использованием OGREnvelope
     * \param[in] geom Геометрия OGR. Не допускается nullptr
     * \return Ширина геометрии
     */
    double EnvelopeWidth(OGRGeometry *geom);

    /*!
     * \brief Высота Bounding Box
     * \param e Исходный bounding box
     * \return Высота Bounding Box
     */
    double EnvelopeHeight(OGREnvelope &e);

    /*!
     * \brief Расчет высоты геометрии с использованием OGREnvelope
     * \param[in] geom Геометрия OGR. Не допускается nullptr
     * \return Высота геометрии
     */
    double EnvelopeHeight(OGRGeometry *geom);

    /*!
    @}
    */

    //-----------------------------------------------------------

    /*!
    \defgroup gdu_files Утилиты для работы с файлами и слоями
    \ingroup gdalutilities
    \brief Утилиты для работы с файлами и слоями
    @{
    */

    /*!
    \brief Создание набора данных GDAL из файла shp.
    \details Поддерживаются только файлы shp. Если будет указан
    файл другого типа, то произойдет ошибка.
    Если не удается загрузить драйвер ESRI Shapefile или
    не удается открыть файл, также произойдет ошибка.
    \param[in] fname Имя файла
    \return Новый dataset в случае успеха. В случае ошибки
    возвращается nullptr.
    */
    GDALDataset *OpenSHPFile(const std::string fname);

    /*!
    \brief Загрузка наборов данных сразу из нескольких файлов shp.
    \details Если происходит ошибка при открытии какого-либо файла, то
    в inputDatasets не добавляется набор данных, ассоциированный с этим файлом.
    \param[in] files Список файлов, из которых нужно создать наборы данных GDAL.
    Его можно оставлять пустым, тогда не создастся новых наборов данных.
    \param[out] inputDatasets Вектор, в который добавляются новые наборы
    данных.
    */
    void LoadDatasets(StringList files, std::vector<GDALDataset*> &inputDatasets);

    /*!
    \brief Освобождение набора данных GDAL
    \param[in] inputDatasets Вектор наборов данных. Не допускается,
    чтобы хотя бы один из элементов вектора был nullptr.
    */
    void FreeDatasets(std::vector<GDALDataset*> &inputDatasets);

    /*!
    \brief Добавление полигонов со слоя в коллекцию полигонов.
    \details Если в слое нет геометрических объектов, то ничего
    не происходит. Если слой содержит не полигоны (wkbPolygon),
    то в коллекцию ничего не добавляется.
    \param[out] polygons Коллекция, в которую добавляются полигоны.
    Не допускается nullptr.
    \param[in] layer Слой, полигоны которого добавляются в polygons.
    */
    void AddPolygonsFromLayer(OGRMultiPolygon* &polygons, OGRLayer *layer);

    /*!
    \brief Создание коллекции полигонов из нескольких файлов.
    \details Из каждого файла в списке files загружается слой с названием
    layerName, затем полигоны из этого слоя добавляются в коллекцию полигонов.
    Если файл не существует, или в нем нет указанного слоя, то файл пропускается.
    Если в нужном слое файла нет полигонов, или слой содержит
    не полигоны, то геометрия с данного файла не добавляется.
    \param[in] files Список файлов. Может быть пустым.
    \param[in] layerName Название слоя, который будет загружаться из файла.
    \return Новая коллекция полигонов. Не может быть nullptr.
    Может быть пустой.
    */
    OGRMultiPolygon *FetchGeometryFromFiles(StringList files, std::string layerName);

    /*!
     * \brief Запись геометрической коллекции в файл
     * \param[in] collection Коллекция, которую необходимо сохранить
     * \param[in] fileName Имя файла, в который необходимо сохранить
     * \param[in] layerName Название слоя
     * \param[in] driverName Драйвер файла
     * \return true в случае удачной записи, false в противном случае
     */
    bool WriteGeometryCollectionToFile(OGRGeometryCollection *collection,
                                       std::string fileName,
                                       std::string layerName = std::string("Default layer"),
                                       std::string driverName = std::string("ESRI Shapefile"));
    /*!
    @}
    */

    /*!
    \brief Красивый вывод прогресса в консоль
    \details Кросплатформенная функция вывода красивого
    прогрессбара в консоль. Прогресс указывается в процентах.
    \param[in] progress Прогресс на текущем такте.
    \param[in] prev_progress Прогресс на предыдущем такте.
    */
    void FancyProgress(float &progress, float &prev_progress);

    /*!
    \brief Консольный индикатор прогресса
    \details Класс простого консольного индикатора прогресса.
    Этот класс можно использовать для индикации работы любой
    продолжительной функции. Использует функцию FancyProgress.
    */
    class ProgressIndicator
    {
        ///Прогресс в процентах
        float progress;
        ///Прогресс на предыдущем такте в процентах
        float prev_progress;
        ///Число операций
        int op_cnt;
        ///Максимальное число операций
        int max_op;
        ///Название прогресс бара
        std::string caption;
    public:
        /*!
        \brief Конструктор
        \param[in] max_operations Максимальное число операций,
        выполняемое в вычислительном процессе.
        \param[in] _caption Название прогрессбара.
        */
        ProgressIndicator(int max_operations, std::string
            _caption = "progress");
        /*!
        \brief Инкремент числа операций
        */
        void incOperationCount();
    };

    /*!
    @}
    */
}

#endif
