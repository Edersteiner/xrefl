namespace math {

FUNCTION()
int compute(float x, const char* name, int* out);

FUNCTION()
auto trailing(int a) -> const char*;

FUNCTION()
void noargs();

}

FUNCTION()
double global_fn(double);
