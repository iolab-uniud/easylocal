int easylocal_odr_a();
int easylocal_odr_b();

static_assert(__cplusplus >= 202302L, "EasyLocal needs C++23");

int main()
{
    return (easylocal_odr_a() + easylocal_odr_b() == 3) ? 0 : 1;
}
