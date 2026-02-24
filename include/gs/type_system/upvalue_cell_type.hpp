#pragma once

#include "gs/type_system/type_base.hpp"

namespace gs {

// UpvalueCellObject is the runtime "reference box" used by closures.
// Captured outer locals are wrapped in this cell so multiple frames/lambdas
// share one mutable value instead of copying by value.
class UpvalueCellObject : public Object {
public:
    UpvalueCellObject(const Type& typeRef, Value value);

    const Type& getType() const override;
    Value& value();
    const Value& value() const;

private:
    const Type* type_;
    Value value_;
};

class UpvalueCellType : public Type {
public:
    // Type tag for the closure cell object. Kept as an object type so it can
    // participate in GC and write barriers like other heap references.
    UpvalueCellType();
    const char* name() const override;
    std::string __str__(Object& self, const ValueStrInvoker& valueStr) const override;
};

} // namespace gs
