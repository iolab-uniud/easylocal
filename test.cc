template <typename A>
struct X { int j; };

struct Y1 : public X<int> {};

struct B {};

struct Y2 : public X<B> {};

int main() {

  Y1 x;
  Y2 y;

  return 0;
}

