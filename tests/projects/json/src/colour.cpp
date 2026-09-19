#include "model.h"

// Hand-written pair for a type xrefl does not reflect. Overload resolution
// finds it for every type containing a Colour.
namespace game {

yyjson_mut_val* xrefl_to_json(yyjson_mut_doc* doc, const Colour& value) {
    yyjson_mut_val* array = yyjson_mut_arr(doc);
    yyjson_mut_arr_add_real(doc, array, value.r);
    yyjson_mut_arr_add_real(doc, array, value.g);
    yyjson_mut_arr_add_real(doc, array, value.b);
    return array;
}

bool xrefl_from_json(yyjson_val* value, Colour& out) {
    if (!yyjson_is_arr(value) || yyjson_arr_size(value) != 3) return false;
    out.r = static_cast<float>(yyjson_get_num(yyjson_arr_get(value, 0)));
    out.g = static_cast<float>(yyjson_get_num(yyjson_arr_get(value, 1)));
    out.b = static_cast<float>(yyjson_get_num(yyjson_arr_get(value, 2)));
    return true;
}

}  // namespace game
