#pragma once

#include <functional>
#include <string>
#include <vector>


// For user convenience when registering their own transforms inside build<Schema>(rt):

// information for developers: the STORAGE variable is created automatically inside the lambda
// by getting it from the runtime container passed to build<Schema> such that the user does not
// have to care about it. 
// For information regarding the type definitions of Transform2One and Transform2N, see 
// field_transforms.cppm


#define TRANSFORM2ONE(NAME, ARGS, OUT_VAL, STORAGE)                                  \
  rt.getTransformRegistry().registerTransform(                                       \
    #NAME,                                                                           \
    field_transforms::Transform2One{                                                 \
      [&](const field_transforms::Args& ARGS, field_transforms::Out1& OUT_VAL) -> void {     \
        auto& STORAGE = rt.getStorage();

#define TRANSFORM2MANY(NAME, ARGS, OUT_VALS, STORAGE)                                \
  rt.getTransformRegistry().registerTransform(                                       \
    #NAME,                                                                           \
    field_transforms::Transform2N{                                                   \
      [&](const field_transforms::Args& ARGS, field_transforms::OutN& OUT_VALS) -> void { \
        auto& STORAGE = rt.getStorage();

#define TRANSFORM_END                                                                \
      }                                                                              \
    }                                                                                \
  );

