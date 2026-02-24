#include "gs/type_system/upvalue_cell_type.hpp"

namespace gs {

UpvalueCellObject::UpvalueCellObject(const Type& typeRef, Value value)
    : type_(&typeRef), value_(value) {}

const Type& UpvalueCellObject::getType() const {
    return *type_;
}

Value& UpvalueCellObject::value() {
    return value_;
}

const Value& UpvalueCellObject::value() const {
    return value_;
}

UpvalueCellType::UpvalueCellType() {
}

const char* UpvalueCellType::name() const {
    return "UpvalueCell";
}

std::string UpvalueCellType::__str__(Object& self, const ValueStrInvoker& valueStr) const {
    auto* cell = dynamic_cast<UpvalueCellObject*>(&self);
    if (!cell) {
        return "[UpvalueCell]";
    }
    return std::string("[UpvalueCell ") + valueStr(cell->value()) + "]";
}

} // namespace gs
