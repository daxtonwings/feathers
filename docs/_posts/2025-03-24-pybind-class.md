---
title: "笔记：pybind11（一） 封装多态类型"
---

> https://pybind11.readthedocs.io/en/stable/advanced/classes.html#classes

## 一、pybind封装基本类型

```c++
#include <pybind11/pybind11.h>

#include <format>
#include <string>

namespace py = pybind11;
using std::string;

#define MODULE_NAME demo

struct Pet
{
    Pet(string name)
        : name_(std::move(name)), age_(0) {}
    Pet(const std::string& name, int age)
        : name_(name), age_(age) {}
    void setName(const std::string& name) { name_ = name; }
    const std::string& getName() const { return name_; }

    void set(int age) { age_ = age; }
    void set(const string& name) { name_ = name; }
    int age() const { return age_; }

private:
    std::string name_;
    int age_;
};

PYBIND11_MODULE(MODULE_NAME, m)
{
    py::class_<Pet> pet(
        m,
        "Pet",
        py::dynamic_attr() // enable __dict__
    );

    pet.def(py::init<std::string>());
    pet.def(py::init<std::string, int>());
    
    pet.def("setName", &Pet::setName)
        .def("getName", &Pet::getName)
        .def("__repr__", [](const Pet& a) {
            return std::format("carena.Pet({},{})", a.getName(), a.age());
        })
        .def_property("name", &Pet::getName, &Pet::setName);
    
    // overload different method Pet::set
    pet.def("set", py::overload_cast<int>(&Pet::set), "set age")
        .def("set", py::overload_cast<const string&>(&Pet::set), "set name");
}
```


## 二、基类指针转换为具体类型指针

发送c++对象至python对象转换时，pybind11将c++的多态类型的基类指针自动转换为具体类型的指针

1. 下面`poly_pet_store()`返回的是python对象可以调用`bark()`方法
2. 如果`PolyPet`基类不是多态基类（没有虚析构函数），那么`poly_pet_store()`返回的python对象不能调用`bark()`方法


```c++
#include <pybind11/pybind11.h>

#include <format>
#include <string>

namespace py = pybind11;
using std::string;

#define MODULE_NAME demo

struct PolyPet
{
    virtual ~PolyPet() = default;
};

struct PolyDog : PolyPet
{
    std::string bark() const { return "woof!"; }
};

PYBIND11_MODULE(MODULE_NAME, m)
{
    py::class_<PolyPet> poly_pet(m, "PolyPet");

    py::class_<PolyDog> poly_dog(m, "PolyDog", poly_pet);
    poly_dog.def("bark", &PolyDog::bark);

    // exp: pybind11将指向多态类型基类的指针，转换为具体的类型对象。
    m.def("poly_pet_store", []() { return std::unique_ptr<PolyPet>(new PolyDog()); });
}
```

## 三、python可继承的多态类型

```c++
#include <pybind11/pybind11.h>

#include <string>
#include <format>

namespace py = pybind11;
using std::string;

#define MODULE_NAME carena

class Animal
{
public:
    virtual ~Animal() {}
    virtual std::string go(int n_times) = 0;
    virtual std::string name() { return "unknown"; }
};

class PyAnimal : public Animal
{
public:
    using Animal::Animal;

    std::string go(int n_times) override
    {
        PYBIND11_OVERLOAD_PURE(
            std::string, // return type
            Animal,      // parent class
            go,
            n_times
        );
    }
    std::string name() override
    {
        PYBIND11_OVERLOAD(std::string, Animal, name, );
    }
};

class Dog : public Animal
{
public:
    std::string go(int n_times) override
    {
        std::string result;
        for ( int i = 0; i < n_times; ++i )
            result += "woof! ";
        return result;
    }

    virtual std::string bark() { return "woof!"; }
};

class PyDog : public Dog
{
public:
    using Dog::Dog;
    std::string go(int n_times) override
    {
        PYBIND11_OVERLOAD(std::string, Dog, go, n_times);
    }
    std::string name() override
    {
        PYBIND11_OVERLOAD(std::string, Dog, name, );
    }
    std::string bark() override
    {
        PYBIND11_OVERLOAD(std::string, Dog, bark, );
    }
};

std::string call_go(Animal* animal)
{
    return animal->go(3);
}


PYBIND11_MODULE(MODULE_NAME, m)
{
    py::class_<Animal, PyAnimal> animal(m, "Animal");
    animal.def(py::init())
        .def("go", &Animal::go);

    //    py::class_<Dog, Animal, PyDog> dog(m, "Dog");
    py::class_<Dog, Animal> dog(m, "Dog");
    dog.def(py::init())
        .def("go", &Dog::go)
        .def("bark", &Dog::bark);

    m.def("call_go", &call_go);

}
```

### 3.1 PYBIND11_OVERLOAD[_PURE]

这个宏在C++虚函数的作用：“如果该函数被Python重写了，则调用Python的方法”

比如下面这个宏方法，以及展开后的“伪代码”

宏参数意义：
- std::string，执行结果的`.cast`转型
- `Animal`, 在python未重载时回退使用，调用c++提供的`Animal::go`方法，
- `go`被重写的函数名，也是python查找的函数名
- `n_times`: 参数

```c++
PYBIND11_OVERLOAD_PURE(
    std::string,   // 返回类型
    Animal,        // C++ 父类类型
    go,            // 函数名（写法：无引号，无括号）
    n_times        // 参数（按顺序列出）
);
// 展开的逻辑代码

pybind11::gil_scoped_acquire gil;
pybind11::function override = pybind11::get_override(this, "go");

if (!override)
    throw pybind11::error_already_set("Tried to call pure virtual function 'Animal::go'");

auto result = override(n_times);  // 这里用的是参数
return result.cast<std::string>(); // 返回类型是 std::string
```

### 3.2 py::class_ 以及 Trampoline

`py::class_<>`是将c++类型包装为python类型的关键代码，通常有几种使用形式

1. `py::class_<T>(m, "Dog");`
   - 只绑定类型本身的普通类型
2. `py::class_<T, Base>(m, "Dog");`
    - 绑定类型、基类
    - 支持多态、向上类型转换
    - 不支持**Python重写虚函数**
3. `py::class_<T, Base, Trampoline>(m, "Dog");`
    - 目标类型、基类、trampoline类型
    - 支持多态，支持python覆盖虚函数
    - Trampoline类型使用`PYBIND11_OVERLOAD[_PURE]`宏
4. `py::class_<T, std::shared_ptr<T>[, Base][, Trampoline]>(m, "Dog");`
    - 目标类型、智能指针、（可选基类）、（可选Trampoline）
    - 可以指定管理对象声明周期的指针对象，默认是`unique_ptr`
   
