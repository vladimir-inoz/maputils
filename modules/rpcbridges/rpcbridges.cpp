#include <rpcbridges.h>

#include <rpcbridges_dev.h>

using namespace GDALUtilities::Boilerplates;

OGC *BridgesRPC::
exec(OGRGeometryCollection * input, double ratio, double max_distance)
{
    BridgeConstructor c(input, ratio, max_distance);
    return c.exec();
}
