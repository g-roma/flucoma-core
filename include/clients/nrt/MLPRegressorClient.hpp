#pragma once

#include "DataSetClient.hpp"
#include "NRTClient.hpp"
#include "algorithms/MLP.hpp"
#include "algorithms/SGD.hpp"
#include <string>

namespace fluid {
namespace client {

class MLPRegressorClient : public FluidBaseClient,
                           OfflineIn,
                           OfflineOut,
                           ModelObject,
                           public DataClient<algorithm::MLP> {


  enum { kHidden, kActivation, kIter, kRate, kMomentum, kBatchSize };

public:
  using string = std::string;
  using BufferPtr = std::shared_ptr<BufferAdaptor>;
  using IndexVector = FluidTensor<index, 1>;
  using StringVector = FluidTensor<string, 1>;
  using DataSet = FluidDataSet<string, double, 1>;
  template <typename T> Result process(FluidContext &) { return {}; }

  static constexpr std::initializer_list<index>  HiddenLayerDefaults = {3, 3};

  FLUID_DECLARE_PARAMS(
      LongArrayParam("hidden","Hidden layer sizes", HiddenLayerDefaults),
      EnumParam("activation", "Activation function", 0, "Identity", "Sigmoid",
                "ReLU", "Tanh"),
      LongParam("maxiter", "Max iterations", 100),
      FloatParam("rate", "Learning rate", 0.0001, Min(0.0), Max(0.9)),
      FloatParam("momentum", "Momentum", 0.9, Min(0.0), Max(0.99)),
      LongParam("batchsize", "Batch size", 50)
    );

  MLPRegressorClient(ParamSetViewType &p) : mParams(p) {}

  MessageResult<double> fit(DataSetClientRef source, DataSetClientRef target) {
    auto sourceClientPtr = source.get().lock();
    if (!sourceClientPtr)
      return Error<double>(NoDataSet);
    auto sourceDataSet = sourceClientPtr->getDataSet();
    if (sourceDataSet.size() == 0)
      return Error<double>(EmptyDataSet);

    auto targetClientPtr = target.get().lock();
    if (!targetClientPtr)
      return Error<double>(NoDataSet);
    auto targetDataSet = targetClientPtr->getDataSet();
    if (targetDataSet.size() == 0)
      return Error<double>(EmptyDataSet);
    if (sourceDataSet.size() != targetDataSet.size())
      return Error<double>(SizesDontMatch);

    if (mTracker.changed(get<kHidden>(), get<kActivation>())){
      mAlgorithm.init(sourceDataSet.pointSize(), targetDataSet.pointSize(),
                    get<kHidden>(), get<kActivation>());
    }
    DataSet result(1);
    auto ids = sourceDataSet.getIds();
    auto data = sourceDataSet.getData();
    auto tgt = targetDataSet.getData();
    algorithm::SGD sgd;
    double error = sgd.train(mAlgorithm, data, tgt, get<kIter>(),
                             get<kBatchSize>(), get<kRate>(), get<kMomentum>());
    return error;
  }

  MessageResult<void> predict(DataSetClientRef srcClient,
                              DataSetClientRef destClient, index layer) {
    auto srcPtr = srcClient.get().lock();
    auto destPtr = destClient.get().lock();
    if (!srcPtr || !destPtr)
      return Error(NoDataSet);
    auto srcDataSet = srcPtr->getDataSet();
    if (srcDataSet.size() == 0)
      return Error(EmptyDataSet);
    if (!mAlgorithm.trained())
      return Error(NoDataFitted);
    if (srcDataSet.dims() != mAlgorithm.dims())
      return Error(WrongPointSize);

    // default 0 is final layer, so 1-indexed for the rest
    if(layer <= 0 || layer > mAlgorithm.size()) layer = mAlgorithm.size();
    layer -= 1;

    StringVector ids{srcDataSet.getIds()};
    RealMatrix output(srcDataSet.size(), mAlgorithm.outputSize(layer));
    mAlgorithm.process(srcDataSet.getData(), output, layer);
    FluidDataSet<string, double, 1> result(ids, output);
    destPtr->setDataSet(result);
    return OK();
  }

  MessageResult<void> predictPoint(BufferPtr in, BufferPtr out, index layer) {
    if (!in || !out)
      return Error(NoBuffer);
    BufferAdaptor::Access inBuf(in.get());
    BufferAdaptor::Access outBuf(out.get());
    if (!inBuf.exists())
      return Error(InvalidBuffer);
    if (!outBuf.exists())
      return Error(InvalidBuffer);
    if (inBuf.numFrames() != mAlgorithm.dims())
      return Error(WrongPointSize);
    if (!mAlgorithm.trained())
      return Error(NoDataFitted);

    if(layer <= 0 || layer > mAlgorithm.size()) layer = mAlgorithm.size();
      layer -= 1;

    Result resizeResult =
        outBuf.resize(mAlgorithm.outputSize(layer), 1, inBuf.sampleRate());
    if (!resizeResult.ok())
      return Error(BufferAlloc);
    RealVector src(mAlgorithm.dims());
    RealVector dest(mAlgorithm.outputSize(layer));
    src = inBuf.samps(0, mAlgorithm.dims(), 0);

    mAlgorithm.processFrame(src, dest, layer);
    outBuf.samps(0) = dest;
    return {};
  }

  FLUID_DECLARE_MESSAGES(makeMessage("fit", &MLPRegressorClient::fit),
                         makeMessage("predict", &MLPRegressorClient::predict),
                         makeMessage("predictPoint",
                                     &MLPRegressorClient::predictPoint),
                         makeMessage("cols", &MLPRegressorClient::dims),
                         makeMessage("size", &MLPRegressorClient::size),
                         makeMessage("load", &MLPRegressorClient::load),
                         makeMessage("dump", &MLPRegressorClient::dump),
                         makeMessage("write", &MLPRegressorClient::write),
                         makeMessage("read", &MLPRegressorClient::read));

private:
  ParameterTrackChanges<IndexVector, index> mTracker;
};

using NRTThreadedMLPRegressorClient =
    NRTThreadingAdaptor<ClientWrapper<MLPRegressorClient>>;

} // namespace client
} // namespace fluid
