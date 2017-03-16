#include "tiles.h"

#include <cassert>

#include <gdal_priv.h>
#include <ogrsf_frmts.h>

using namespace Tiles;

//Инициализируем счетчик тайлов
int Tile::counter = 0;

Tile::Tile(OGRGeometry * g, int group)
{
    assert(g);
    //геометрию добавляем
    m_geometry = g;
    //номер тайла - из статического счетчика
    m_index = counter++;
    m_group = group;
}

TileCollection::TileCollection()
{

}

void TileCollection::addTile(Tile * t)
{
    assert(t);

    //создаем новый элемент коллекции
    std::pair< int, std::shared_ptr<Tile> > newNode
        = std::make_pair(
            //ключ - индекс тайла
            t->index(),
            //значение - умный указатель на тайл
            std::shared_ptr<Tile>(t)
            );

    //добавляем элемент
    collection.insert(newNode);
}

int TileCollection::countGroups()
{
    //счетчик групп
    int ngroups = 0;

    //ищем максимум среди номеров групп в тайлах коллекции
    for (auto i = collection.begin();i != collection.end();i++)
    {
        //текущая группа
        int curGroup = (*i).second->group();
        
        if (curGroup > ngroups)
            ngroups = curGroup;
    }

    return ngroups;
}

int TileCollection::countTilesInGroup(int grp)
{
    //счетчик тайлов
    int ntiles = 0;

    //перебираем всю коллекцию
    //если тайл находится в группе, инкрементируем
    //счетчик
    for (auto i = collection.begin();i != collection.end();i++)
    {
        if ((*i).second->group() == grp) ntiles++;
    }

    return ntiles;
}

bool TileCollection::inSameGroup(int idx1, int idx2)
{
    return
        (collection.at(idx1)->group() ==
            collection.at(idx2)->group());
}
