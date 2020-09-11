/*
Part of the Fluid Corpus Manipulation Project (http://www.flucoma.org/)
Copyright 2017-2019 University of Huddersfield.
Licensed under the BSD-3 License.
See license.md file in the project root for full license information.
This project has received funding from the European Research Council (ERC)
under the European Union’s Horizon 2020 research and innovation programme
(grant agreement No 725899).
*/
#pragma once

#include "../common/FluidBaseClient.hpp"
#include "../common/FluidNRTClientWrapper.hpp"
#include "../common/ParameterConstraints.hpp"
#include "../common/ParameterTypes.hpp"

namespace fluid {
namespace client{

static constexpr std::initializer_list<index> BufSelectionDefaults = {-1};


class BufScaleClient :    public FluidBaseClient,
                          public OfflineIn,
                          public OfflineOut
{

  enum BufSelectParamIndex {
    kSource,
    kDest, 
    kInLow,
    kInHigh,
    kOutLow, 
    kOutHigh 
  };
  public:
  
  FLUID_DECLARE_PARAMS(InputBufferParam("source", "Source Buffer"),
                       BufferParam("destination","Destination Buffer"), 
                       FloatParam("inlo","Input Low Range",0),
                       FloatParam("inhi","Input High Range",1),
                       FloatParam("outlo","Output Low Range",0),
                       FloatParam("outhi","Output High Range",1));  
  
  BufScaleClient(ParamSetViewType& p) : mParams(p) {}                   
  
  template <typename T>
  Result process(FluidContext&)
  {
    
    if (!get<kSource>().get())
      return {Result::Status::kError, "No input buffer supplied"};

    if (!get<kDest>().get())
      return {Result::Status::kError, "No output buffer supplied"};

    BufferAdaptor::ReadAccess source(get<kSource>().get());
    BufferAdaptor::Access     dest(get<kDest>().get());

    if (!source.exists())
      return {Result::Status::kError, "Input buffer not found"};

    if (!source.valid())
      return {Result::Status::kError, "Can't access input buffer"};

    if (!dest.exists())
      return {Result::Status::kError, "Output buffer not found"};
    
    double scale = (get<kOutHigh>()-get<kOutLow>())/(get<kInHigh>()-get<kInLow>()); 
    double offset = get<kOutLow>() - ( scale * get<kInLow>() ); 

    FluidTensor<float, 2> tmp(source.numFrames(),source.numChans());
  
    for(index i = 0; i < source.numChans(); ++i)
      tmp.col(i) = source.samps(i);
    
    tmp.apply([&offset,&scale](float& x){
        x *= scale;
        x += offset; 
    }); 
        
    dest.resize(source.numFrames(),source.numChans(),source.sampleRate());
    
    for(index i = 0; i < source.numChans(); ++i)
       dest.samps(i) = tmp.col(i);
    
    return {};
  }                  
}; 

using NRTThreadedBufferScaleClient =
    NRTThreadingAdaptor<ClientWrapper<BufScaleClient>>;
}//client
}//fluid