#include <iterator>
#include <memory>
#include <utility>

#include "precice/impl/DataContext.hpp"
#include "utils/EigenHelperFunctions.hpp"

namespace precice::impl {

logging::Logger DataContext::_log{"impl::DataContext"};

DataContext::DataContext(mesh::PtrData data, mesh::PtrMesh mesh)
{
  PRECICE_ASSERT(data);
  _providedData = data;
  PRECICE_ASSERT(mesh);
  _mesh = mesh;
}

std::string DataContext::getDataName() const
{
  PRECICE_ASSERT(_providedData);
  return _providedData->getName();
}

void DataContext::resetInitialGuesses()
{
  for (auto &kv : _initialGuesses) {
    kv.second.setZero();
  }
}

int DataContext::getDataDimensions() const
{
  PRECICE_ASSERT(_providedData);
  return _providedData->getDimensions();
}

int DataContext::getSpatialDimensions() const
{
  PRECICE_ASSERT(_providedData);
  return _providedData->getSpatialDimensions();
}

std::string DataContext::getMeshName() const
{
  PRECICE_ASSERT(_mesh);
  return _mesh->getName();
}

int DataContext::getMeshVertexCount() const
{
  PRECICE_ASSERT(_mesh);
  return _mesh->vertices().size();
}

MeshID DataContext::getMeshID() const
{
  PRECICE_ASSERT(_mesh);
  return _mesh->getID();
}

bool DataContext::hasGradient() const
{
  PRECICE_ASSERT(_providedData);
  return _providedData->hasGradient();
}

void DataContext::appendMapping(MappingContext mappingContext)
{
  PRECICE_ASSERT(mappingContext.fromData);
  PRECICE_ASSERT(mappingContext.toData);
  // Make sure we don't append a mapping twice
#ifndef NDEBUG
  for (auto &context : _mappingContexts) {
    PRECICE_ASSERT(!((context.mapping == mappingContext.mapping) && (context.fromData == mappingContext.fromData) && (context.fromData == mappingContext.toData)), "The appended mapping already exists.");
  }
#endif
  _mappingContexts.emplace_back(mappingContext);
  PRECICE_ASSERT(mappingContext.fromData == _providedData || mappingContext.toData == _providedData, "Either fromData or toData has to equal _providedData.");
  PRECICE_ASSERT(mappingContext.fromData->getName() == getDataName());
  PRECICE_ASSERT(mappingContext.toData->getName() == getDataName());
}

bool DataContext::hasMapping() const
{
  return hasReadMapping() || hasWriteMapping();
}

void DataContext::clearToData()
{
  PRECICE_TRACE(getMeshName(), getDataName());
  PRECICE_ASSERT(hasMapping());
  for (auto &context : _mappingContexts) {
    /// The sample() must be overwritte by a following mapping
    context.toData->timeStepsStorage().clear();
  }
}

void DataContext::trimToDataAfter(double t)
{
  PRECICE_TRACE(getMeshName(), getDataName(), t);
  PRECICE_ASSERT(hasMapping());
  for (auto &context : _mappingContexts) {
    context.toData->timeStepsStorage().trimAfter(t);
  }
}

int DataContext::mapData(std::optional<double> after)
{
  PRECICE_TRACE(getMeshName(), getDataName());
  PRECICE_ASSERT(hasMapping());

  int executedMappings{0};

  // Execute the mappings
  for (auto &context : _mappingContexts) {
    PRECICE_ASSERT(!context.fromData->stamples().empty(),
                   "There must be samples at this point!");

    // linear lookup should be sufficient here
    const auto timestampExists = [times = context.toData->timeStepsStorage().getTimes()](double lookup) -> bool {
      return std::any_of(times.data(), std::next(times.data(), times.size()), [lookup](double time) {
        return math::equals(time, lookup);
      });
    };

    auto &mapping = *context.mapping;

    const auto dataDims = context.fromData->getDimensions();

    for (const auto &stample : context.fromData->stamples()) {
      // skip stamples before given time
      if (after && math::smallerEquals(stample.timestamp, *after)) {
        PRECICE_DEBUG("Skipping stample t={} (not after {})", stample.timestamp, *after);
        continue;
      }
      // skip existing stamples
      if (timestampExists(stample.timestamp)) {
        PRECICE_DEBUG("Skipping stample t={} (exists)", stample.timestamp);
        continue;
      }

      PRECICE_INFO("Mapping \"{}\" for t={} from \"{}\" to \"{}\"",
                   getDataName(), stample.timestamp, mapping.getInputMesh()->getName(), mapping.getOutputMesh()->getName());
      time::Sample outSample{
          dataDims,
          Eigen::VectorXd::Zero(dataDims * mapping.getOutputMesh()->vertices().size())};

      if (mapping.requiresInitialGuess()) {
        const FromToDataIDs key{context.fromData->getID(), context.toData->getID()};
        mapping.map(stample.sample, outSample.values, _initialGuesses[key]);
      } else {
        mapping.map(stample.sample, outSample.values);
      }

      PRECICE_DEBUG("Mapped values (t={}) = {}", stample.timestamp, utils::previewRange(3, outSample.values));

      // Store data from mapping buffer in storage
      context.toData->setSampleAtTime(stample.timestamp, std::move(outSample));
      ++executedMappings;
    }
  }
  return executedMappings;
}

bool DataContext::hasReadMapping() const
{
  return std::any_of(_mappingContexts.begin(), _mappingContexts.end(), [this](auto &context) { return context.toData == _providedData; });
}

bool DataContext::hasWriteMapping() const
{
  return std::any_of(_mappingContexts.begin(), _mappingContexts.end(), [this](auto &context) { return context.fromData == _providedData; });
}

bool DataContext::isValidVertexID(const VertexID id) const
{
  PRECICE_ASSERT(_mesh);
  return _mesh->isValidVertexID(id);
}

} // namespace precice::impl
