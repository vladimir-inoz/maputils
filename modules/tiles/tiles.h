/*!
\file
\brief Библиотека с описанием типа тайлов
\author Владимир Иноземцев
\version 1.0
*/

#ifndef TILES_H
#define TILES_H

class OGRGeometry;
#include <map>
#include <memory>

namespace Tiles
{
    ///Тип геометрии тайла
    class Tile
    {
        ///статический счетчик индексов
        static int counter;
        ///указатель на геометрию GDAL
        OGRGeometry *m_geometry;
        ///уникальный индекс тайла
        int m_index;
        ///группа тайла
        int m_group;
    public:
        explicit Tile(OGRGeometry *g, int group);
        int index() { return m_index; }
        int group() { return m_group; }
        OGRGeometry *geometry() { return m_geometry; }
    };

    ///массив тайлов
    ///позволяет быстро найти тайл
    ///по индексу

    typedef std::map< int, std::shared_ptr<Tile> > TileMap;
    typedef TileMap::iterator TileMapIterator;

    class TileCollection
    {
        //используются умные указатели для автоматического
        //управления памятью
        //ключ - индекс тайла
        ///ассоциативный массив тайлов
        std::map< int, std::shared_ptr<Tile> > collection;
    public:
        explicit TileCollection();
        ///Добавление нового тайла в коллекцию. Управление
        ///памятью тайла теперь осуществляет TileCollection
        void addTile(Tile *t);
        ///Число групп
        int countGroups();
        ///Число тайлов в группе
        int countTilesInGroup(int grp);
        ///Общее число тайлов
        int size() { return collection.size(); }
        ///Тайл по индексу
        ///Не удалять, используется внутренней логикой!
        Tile *tileByIndex(int idx) { return collection.at(idx).get(); }
        ///Проверка, находятся ли тайлы в одной группе
        bool inSameGroup(int idx1, int idx2);
        ///Итераторы
        TileMapIterator begin() { return collection.begin(); }
        TileMapIterator end() { return collection.end(); }
    };
    
}
#endif
