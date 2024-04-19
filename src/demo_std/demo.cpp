#include <iostream>
//#include <memory>
//#include <boost/enable_shared_from_this.hpp>


class Widget :
    public std::enable_shared_from_this<Widget> {
public:
  typedef std::shared_ptr<Widget> pointer;

  static pointer create() {
    return pointer(new Widget());
  }


  ~Widget() {
    std::cout << "Widget destructor run" << std::endl;
  }

private:
  Widget() {
    std::cout << "Widget constructor run" << std::endl;
  }

};

int main() {
  Widget::pointer p = Widget::create();
  std::shared_ptr<Widget> q = p;

  std::cout << p.use_count() << std::endl;
  std::cout << q.use_count() << std::endl;

  return 0;
}