#include <Eigen/Core>
#include <algorithm>
#include <memory>
#include "logging/LogMacros.hpp"
#include "mapping/AxialGeoMultiscaleMapping.hpp"
#include "mapping/Mapping.hpp"
#include "math/constants.hpp"
#include "mesh/Data.hpp"
#include "mesh/Mesh.hpp"
#include "mesh/SharedPointer.hpp"
#include "mesh/Utils.hpp"
#include "mesh/Vertex.hpp"
#include "testing/TestContext.hpp"
#include "testing/Testing.hpp"

using namespace precice;
using namespace precice::mesh;

BOOST_AUTO_TEST_SUITE(MappingTests)
BOOST_AUTO_TEST_SUITE(AxialGeoMultiscaleMapping)

BOOST_AUTO_TEST_CASE(testConsistentSpread)
{
  /*  The following test works by creating two dimensionally heterogeneous meshes, namely 1D and 3D.
      Then, the data is mapped from the single vertex of the 1D mesh to defined vertices on the circular inlet of the 3D mesh (hence, SPREAD).
      The defined vertices are at certain distances from the center, which enables to predict the expected behavior for Hagen-Poiseuille flow.
      Finally, this expected behavior is tested.  
  */

  PRECICE_TEST(1_rank);
  int dimensions = 3;
  using testing::equals;

  // Create mesh to map from
  PtrMesh inMesh(new Mesh("InMesh", dimensions, testing::nextMeshID()));
  PtrData inData    = inMesh->createData("InData", 3, 0_dataID);
  int     inDataID  = inData->getID();
  Vertex &inVertex0 = inMesh->createVertex(Eigen::Vector3d::Constant(0.0));
  inMesh->allocateDataValues();
  Eigen::VectorXd &inValues = inData->values();
  inValues << 0.0, 0.0, 2.0;

  double radius = 1.0; // radius of the "tube" from or to which the data is mapped, i.e., radius of the circular interface between the two participants

  // Create mesh to map to
  PtrMesh outMesh(new Mesh("OutMesh", dimensions, testing::nextMeshID()));
  PtrData outData    = outMesh->createData("OutData", 3, 2_dataID);
  int     outDataID  = outData->getID();
  Vertex &outVertex0 = outMesh->createVertex(Eigen::Vector3d::Constant(0.0)); // center, equal to incoming mesh node
  Vertex &outVertex1 = outMesh->createVertex(Eigen::Vector3d(1.0, 0.0, 0.0)); // distance of 1.0 = r to center
  Vertex &outVertex2 = outMesh->createVertex(Eigen::Vector3d(0.0, 0.5, 0.0)); // distance of 0.5 = r/2 to center
  outMesh->allocateDataValues();

  // Setup mapping with mapping coordinates and geometry used
  precice::mapping::AxialGeoMultiscaleMapping mapping(mapping::Mapping::CONSISTENT, dimensions, mapping::AxialGeoMultiscaleMapping::SPREAD, radius);
  mapping.setMeshes(inMesh, outMesh);
  BOOST_TEST(mapping.hasComputedMapping() == false);

  // Map data
  mapping.computeMapping();
  mapping.map(inDataID, outDataID);
  const Eigen::VectorXd &outValues = outData->values();

  // Check if data is doubled at center node
  BOOST_TEST(mapping.hasComputedMapping() == true);
  BOOST_TEST(outValues(0) == 2 * inValues(0));
  BOOST_TEST(outValues(1) == 2 * inValues(1));
  BOOST_TEST(outValues(2) == 2 * inValues(2));
  // Check if data at distance = r is equal to zero
  BOOST_TEST(outValues(3) == 0.0);
  BOOST_TEST(outValues(4) == 0.0);
  BOOST_TEST(outValues(5) == 0.0);
  // Check if data at distance = r/2 is 3/2 times invalue data
  BOOST_TEST(outValues(6) == 1.5 * inValues(0));
  BOOST_TEST(outValues(7) == 1.5 * inValues(1));
  BOOST_TEST(outValues(8) == 1.5 * inValues(2));
}

BOOST_AUTO_TEST_CASE(testConsistentCollect)
{
  /*  The following test works by creating two dimensionally heterogeneous meshes, namely 1D and 3D.
      Then, the data is mapped from multiple defined vertices on the circular inlet of the 3D mesh to the single vertex of the 1D mesh (hence, COLLECT).
      The defined vertices are at certain distances from the center, which enables to predict the expected behavior for Hagen-Poiseuille flow.
      Finally, this expected behavior is tested.  
  */

  PRECICE_TEST(1_rank);
  int dimensions = 3;
  using testing::equals;

  // Create mesh to map from
  PtrMesh inMesh(new Mesh("InMesh", dimensions, testing::nextMeshID()));
  PtrData inData    = inMesh->createData("InData", 3, 0_dataID);
  int     inDataID  = inData->getID();
  Vertex &inVertex0 = inMesh->createVertex(Eigen::Vector3d::Constant(0.0)); // center
  Vertex &inVertex1 = inMesh->createVertex(Eigen::Vector3d(1.0, 0.0, 0.0)); // distance of 1.0 = r to center
  Vertex &inVertex2 = inMesh->createVertex(Eigen::Vector3d(0.0, 0.5, 0.0)); // distance of 0.5 = r/2 to center
  inMesh->allocateDataValues();
  Eigen::VectorXd &inValues = inData->values();
  inValues << 0.0, 0.0, 4.0, 0.0, 0.0, 0.0, 0.0, 0.0, 3.0;

  double radius = 1.0; // radius of the "tube" from or to which the data is mapped, i.e., radius of the circular interface between the two participants

  // Create mesh to map to
  PtrMesh outMesh(new Mesh("OutMesh", dimensions, testing::nextMeshID()));
  PtrData outData    = outMesh->createData("OutData", 3, 2_dataID);
  int     outDataID  = outData->getID();
  Vertex &outVertex0 = outMesh->createVertex(Eigen::Vector3d::Constant(0.0)); // equal to center of incoming mesh
  outMesh->allocateDataValues();

  // Setup mapping with mapping coordinates and geometry used
  precice::mapping::AxialGeoMultiscaleMapping mapping(mapping::Mapping::CONSISTENT, dimensions, mapping::AxialGeoMultiscaleMapping::COLLECT, radius);
  mapping.setMeshes(inMesh, outMesh);
  BOOST_TEST(mapping.hasComputedMapping() == false);

  // Map data
  mapping.computeMapping();
  mapping.map(inDataID, outDataID);
  const Eigen::VectorXd &outValues = outData->values();

  // Check if data is averaged at center node
  BOOST_TEST(mapping.hasComputedMapping() == true);
  BOOST_TEST(outValues(0) == (1 / 3.0) * (inValues(0) + inValues(3) + inValues(6)));
  BOOST_TEST(outValues(1) == (1 / 3.0) * (inValues(1) + inValues(4) + inValues(7)));
  BOOST_TEST(outValues(2) == (1 / 3.0) * (inValues(2) + inValues(5) + inValues(8)));

  /*  Due to the nature of the implementation, i.e., not inverting the profile for the mapping but averaging over
      all values on the circular interface to obtain the average velocity, the below tests fail for small numbers of vertices 
      and the tests above are used instead. If COLLECT mapping with inversion of Hagen-Poiseuille profile gets implemented,
      these tests can be used.
      
  BOOST_TEST(outValues(0) == 0.5 * inValues(0));
  BOOST_TEST(outValues(1) == 0.5 * inValues(1));
  BOOST_TEST(outValues(2) == 0.5 * inValues(2));

  // Check if data at distance = r/2 is 2/3 times outvalue data
  BOOST_TEST(outValues(0) == (2 / 3.0) * inValues(3));
  BOOST_TEST(outValues(1) == (2 / 3.0) * inValues(4));
  BOOST_TEST(outValues(2) == (2 / 3.0) * inValues(5));
  */
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()