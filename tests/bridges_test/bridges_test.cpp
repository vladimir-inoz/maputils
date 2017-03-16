#include <gtest/gtest.h>

#include <gdal_priv.h>
#include <ogrsf_frmts.h>

#include <rpcbridges_dev.h>
#include <gdalutilities.h>

#include <boost/graph/adjacency_list.hpp>

//тест на создание сетки
TEST(BridgesCase, Init)
{
    GDALAllRegister();
}

//тест на валидность элементов сетки
TEST(GridCase, Validity)
{
    //создаем тестовые данные
    using namespace GDALUtilities::Boilerplates;
    TempOGC c(newGeometryCollection(), destroy);
    OLR *r = newLinearRing();
    r->addPoint(5.5, 5.5);
    r->addPoint(5.5, 5.7);
    r->addPoint(5.6, 5.7);
    r->addPoint(5.6, 5.5);
    r->addPoint(5.5, 5.5);
    OGRPolygon *p = newPolygon();
    p->addRingDirectly(r);
    c->addGeometryDirectly(p);

    TempOGC grid(GDALUtilities::GenerateGrid(c.get(), 0.001), destroy);
    for (int i = 0;i < c->getNumGeometries();i++)
        //каждый из элементов должен быть валидным
        EXPECT_EQ(true,c->getGeometryRef(i)->IsValid());
}

//тест на число элементов, создаваемых сеткой
//внутри полигона элементы создаваться не должны
//только на границах
TEST(GridCase, Count)
{
    //создаем тестовые данные
    using namespace GDALUtilities::Boilerplates;
    TempOGC c(newGeometryCollection(), destroy);
    OLR *r = newLinearRing();
    r->addPoint(5.5, 5.5);
    r->addPoint(5.5, 5.701);
    r->addPoint(5.6, 5.701);
    r->addPoint(5.6, 5.5);
    r->addPoint(5.5, 5.5);
    OGRPolygon *p = newPolygon();
    p->addRingDirectly(r);
    c->addGeometryDirectly(p);

    //должно создаться 3 элемента, поскольку 5.701 не кратно размеру
    //сетки. Сетка создается так, чтобы гарантированно покрывать
    //всю геометрию.
    TempOGC grid(GDALUtilities::GenerateGrid(c.get(),0.1), destroy);
    int ngeom = grid->getNumGeometries();
    EXPECT_EQ(3,ngeom);
}

//тест на покрытие сеткой исходных полигонов
//сетка должна полностью покрывать прямоугольник
//с размерами, не кратными размеру сетки, при этом
//должна "вылезать" за пределы прямоугольника
TEST(GridCase, CoveringOverlap)
{
    using namespace GDALUtilities::Boilerplates;
    //еще одна тестовая коллекция геометрий из 1-го полигона
    TempOGC c_1(newGeometryCollection(), destroy);
    OLR *r = newLinearRing();
    r->addPoint(5.5, 5.5);
    r->addPoint(5.5, 5.71);
    r->addPoint(5.61, 5.71);
    r->addPoint(5.61, 5.5);
    r->addPoint(5.5, 5.5);
    OGRPolygon *p = newPolygon();
    p->addRingDirectly(r);
    c_1->addGeometryDirectly(p);

    //генерируем новую сетку
    TempOGC grid_1(GDALUtilities::GenerateGrid(c_1.get(), 0.2), destroy);
    //проверяем, чтобы каждый элемент сетки покрывал
    //исходный прямоугольник
    //для этого перебираем все элементы в коллекции grid
    for (int i = 0;i < grid_1->getNumGeometries();i++)
    {
        auto curGrid = grid_1->getGeometryRef(i);
        auto curGeom = c_1->getGeometryRef(0);
        //disjoint должен быть false, потому что у геометрий есть общие точки
        EXPECT_EQ(false,curGrid->Disjoint(curGeom));
        //intersects должен быть true, поскольку intersects=!disjoint
        EXPECT_EQ(true,curGrid->Intersects(curGeom));
        //overlaps должен быть false, поскольку размер исходного
        //полигона не кратен размеру сетки, и некоторые ее
        //элементы "вылезают" за пределы прямоугольника
        EXPECT_EQ(true,curGrid->Overlaps(curGeom));
    }
}



//тесты на деление полигонов сеткой

//тест на число тайлов из полигона
//полигон должен полностью поделиться на тайлы
//поскольку ни один элемент сетки полностью не поместится в него
//в данном случае используется неточность double
TEST(TilesCase, CountWithDoubleBug)
{
    //создаем тестовые данные
    using namespace GDALUtilities::Boilerplates;
    using std::shared_ptr;

    TempOGC c(newGeometryCollection(), destroy);
    OLR *r = newLinearRing();
    r->addPoint(5.5, 5.5);
    r->addPoint(5.5, 5.7);
    r->addPoint(5.6, 5.7);
    r->addPoint(5.6, 5.5);
    r->addPoint(5.5, 5.5);
    OGRPolygon *p = newPolygon();
    p->addRingDirectly(r);
    c->addGeometryDirectly(p);

    //в данном случае из-за неточности double 
    //(5.7 == 5.70000000000000000002
    //generateGrid сделает один лишний элемент, поскольку
    //округляет double до максимального ближайшего int
    //число тайлов - 2 из-за не
    TempOGC grid(GDALUtilities::GenerateGrid(c.get(), 0.1), destroy);
    shared_ptr<BridgesRPC::TileCollection> tiles
        (BridgesRPC::SplitGeometryByGrid(c.get(), grid.get()));
    EXPECT_EQ(2, tiles->size());
}

//тест на число тайлов из полигона
TEST(TilesCase, Count)
{
    //создаем тестовые данные
    using namespace GDALUtilities::Boilerplates;
    using std::shared_ptr;

    TempOGC c(newGeometryCollection(), destroy);
    OLR *r = newLinearRing();
    r->addPoint(5.5, 5.5);
    r->addPoint(5.5, 5.8);
    r->addPoint(5.8, 5.8);
    r->addPoint(5.8, 5.5);
    r->addPoint(5.5, 5.5);
    OGRPolygon *p = newPolygon();
    p->addRingDirectly(r);
    c->addGeometryDirectly(p);

    //но число тайлов все равно должно быть 6
    TempOGC grid(GDALUtilities::GenerateGrid(c.get(), 0.1), destroy);
    shared_ptr<BridgesRPC::TileCollection> tiles
        (BridgesRPC::SplitGeometryByGrid(c.get(), grid.get()));
    //в результате должны быть только полигоны из внешней кромки (8)
    //всего их 9
    EXPECT_EQ(8,tiles->size());
}

//тесты на корректное разделение полигонов на группы

//один полигон - одна группа
TEST(TilesCase, Groups)
{
    //создаем тестовые данные
    using namespace GDALUtilities::Boilerplates;
    using std::shared_ptr;

    TempOGC c(newGeometryCollection(), destroy);
    OLR *r = newLinearRing();
    r->addPoint(5.5, 5.5);
    r->addPoint(5.5, 5.71);
    r->addPoint(5.61, 5.71);
    r->addPoint(5.61, 5.5);
    r->addPoint(5.5, 5.5);
    OGRPolygon *p = newPolygon();
    p->addRingDirectly(r);
    c->addGeometryDirectly(p);

    //структура с группами
    TempOGC grid(GDALUtilities::GenerateGrid(c.get(), 0.1), destroy);
    shared_ptr<BridgesRPC::TileCollection> tiles
        (BridgesRPC::SplitGeometryByGrid(c.get(), grid.get()));
    //один полигон - должна быть одна группа с 6 полигонами
    for (auto i = tiles->begin();i != tiles->end();i++)
        EXPECT_EQ(0,(*i).second->group());
}

//n непересекающихся полигонов - n групп
TEST(TilesCase, MultipleGroups)
{
    //создаем тестовые данные
    using namespace GDALUtilities::Boilerplates;
    using std::shared_ptr;

    const int n = 15;
    double x = 0, y = 0;
    double dx = 0.09;
    double dy = 0.09;

    TempOGC c(newGeometryCollection(), destroy);

    for (int i = 0;i < n;i++)
    {
        OLR *r = newLinearRing();
        r->addPoint(x, y);
        r->addPoint(x, y + dy);
        r->addPoint(x+dx,y+dy);
        r->addPoint(x+dx,y);
        r->addPoint(x, y);
        OGRPolygon *p = newPolygon();
        p->addRingDirectly(r);
        c->addGeometryDirectly(p);
        x += 0.5;
    }

    //сетка
    TempOGC grid(GDALUtilities::GenerateGrid(c.get(), 0.1), destroy);
    //тайлы
    shared_ptr<BridgesRPC::TileCollection> tiles
        (BridgesRPC::SplitGeometryByGrid(c.get(), grid.get()));
    //n полигонов - n групп
    std::vector<bool> exist(n,false);
    for (auto i = tiles->begin();i != tiles->end();i++)
    {
        exist[(*i).second->group()] = true;
    }
    for (auto i = exist.begin();i != exist.end();i++)
        EXPECT_TRUE((*i));
}
