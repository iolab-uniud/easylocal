#include <easylocal/rest/execution.hpp>

int main()
{
    easylocal::rest::execution_pool pool{1, 1};
    return pool.worker_count() == 1 ? 0 : 1;
}
