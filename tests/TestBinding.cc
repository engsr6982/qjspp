#include "TestEngineFixture.hpp"
#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers.hpp"
#include "catch2/matchers/catch_matchers_exception.hpp"
#include "qjspp/Forward.hpp"
#include "qjspp/bind/builder/ClassDefineBuilder.hpp"
#include "qjspp/bind/builder/EnumDefineBuilder.hpp"
#include "qjspp/bind/builder/ModuleDefineBuilder.hpp"
#include "qjspp/runtime/JsEngine.hpp"
#include "qjspp/runtime/Locker.hpp"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <utility>


struct Util {
    static int         add(int a, int b) { return a + b; }
    static std::string append(std::string const& a, std::string const& b) { return a + b; }
    static std::string append(std::string const& a, std::string const& b, std::string const& c) { return a + b + c; }

    static int           foo;
    static std::string   cus;
    static constexpr int bar = 666;
};
std::string Util::cus = "cus";
int         Util::foo = 42;


qjspp::bind::meta::ClassDefine const UtilDefine =
    qjspp::bind::defineClass<void>("Util")
        .function("add", &Util::add)
        .function(
            "append",
            static_cast<std::string (*)(std::string const&, std::string const&)>(Util::append),
            static_cast<std::string (*)(std::string const&, std::string const&, std::string const&)>(Util::append)
        )
        .function("custom", [](qjspp::Arguments const&) -> qjspp::Value { return qjspp::String{"custom"}; })
        .property("foo", &Util::foo)
        .property("bar", &Util::bar)
        .property(
            "cus",
            []() -> qjspp::Value { return qjspp::String{Util::cus}; },
            [](qjspp::Value const& value) { Util::cus = value.asString().value(); }
        )
        .build();


TEST_CASE_METHOD(TestEngineFixture, "Static Binding") {
    qjspp::Locker scope{engine_};
    engine_->registerClass(UtilDefine);

    REQUIRE(engine_->eval("Util.add(1, 2)").asNumber().getInt32() == 3);
    REQUIRE(engine_->eval("Util.append('a', 'b')").asString().value() == "ab");
    REQUIRE(engine_->eval("Util.append('a', 'b', 'c')").asString().value() == "abc");

    REQUIRE(engine_->eval("Util.custom()").asString().value() == "custom");


    // properies
    REQUIRE(engine_->eval("Util.foo").asNumber().getInt32() == 42);
    REQUIRE_NOTHROW(engine_->eval("Util.foo = 128"));
    REQUIRE(Util::foo == 128);

    REQUIRE(engine_->eval("Util.bar").asNumber().getInt32() == 666);
    REQUIRE_THROWS_MATCHES(
        engine_->eval("'use strict'; Util.bar = 777"),
        qjspp::JsException,
        Catch::Matchers::Message("no setter for property")
    );

    REQUIRE(engine_->eval("Util.cus").asString().value() == "cus");
    REQUIRE_NOTHROW(engine_->eval("Util.cus = 'new'"));
    REQUIRE(Util::cus == "new");
}


// Instance binding
class Base {
public:
    virtual ~Base() = default;

    int baseMember = 466;

    virtual std::string type() const { return "Base"; }

    int baseBar() { return 0; }

    static bool        baseTrue() { return true; }
    static std::string name;
};
std::string Base::name = "Base";


class Derived : public Base {
public:
    Derived(int mem) : derivedMember(mem) {}

    int derivedMember = 666;

    std::string type() const override { return "Derived"; }

    static std::string foo;
};
std::string Derived::foo = "Derived::foo";


qjspp::bind::meta::ClassDefine const BaseDefine = qjspp::bind::defineClass<Base>("Base")
                                                      .disableConstructor()
                                                      .instanceProperty("baseMember", &Base::baseMember)
                                                      .instanceMethod("type", &Base::type)
                                                      .instanceMethod("baseBar", &Base::baseBar)
                                                      .property("name", &Base::name)
                                                      .function("baseTrue", &Base::baseTrue)
                                                      .build();

qjspp::bind::meta::ClassDefine const DerivedDefine = qjspp::bind::defineClass<Derived>("Derived")
                                                         .extends(BaseDefine)
                                                         .constructor<int>()
                                                         .instanceProperty("derivedMember", &Derived::derivedMember)
                                                         .instanceMethod("type", &Derived::type)
                                                         .property("foo", &Derived::foo)
                                                         .build();


TEST_CASE_METHOD(TestEngineFixture, "Instance Binding") {
    qjspp::Locker scope{engine_};
    engine_->registerClass(BaseDefine);
    engine_->registerClass(DerivedDefine);

    engine_->globalThis().set("debug", qjspp::Function{[](qjspp::Arguments const& args) -> qjspp::Value {
                                  std::ostringstream oss;
                                  for (int i = 0; i < args.length(); ++i) {
                                      oss << args[i].toString().value();
                                      if (i != args.length() - 1) {
                                          oss << ", ";
                                      }
                                  }
                                  std::cout << "[DEBUG] " << oss.str() << std::endl;
                                  return {};
                              }});
    engine_->eval("debug(Base)");
    engine_->eval("debug(Derived)");

    SECTION("JavaScript new") {
        auto der = engine_->eval("new Derived(114514);");
        REQUIRE(der.isObject());

        auto raw = engine_->getNativeInstanceOf<Derived>(der.asObject(), DerivedDefine);
        REQUIRE(raw != nullptr);
        REQUIRE(raw->derivedMember == 114514);


        // 实例继承链
        REQUIRE(engine_->eval("new Derived(1234).baseBar()").asNumber().getInt32() == 0);
        REQUIRE(engine_->eval("new Derived(1234).baseMember").asNumber().getInt32() == 466);
        REQUIRE(engine_->eval("new Derived(1234).type()").asString().value() == "Derived");

        // 静态属性继承链：
        REQUIRE(engine_->eval("Derived.foo").asString().value() == "Derived::foo"); // Derived::foo
        REQUIRE(engine_->eval("Base.name").asString().value() == "Base");           // Base::name
        REQUIRE(engine_->eval("new Derived(789).name").isUndefined() == true); // 按照标准，静态属性不能通过实例访问

        // TODO: 静态链继承
        CHECK(engine_->eval("Derived.baseTrue()").asBoolean().value() == true);
        CHECK_FALSE(engine_->eval("Derived.name").asString().value() == "Base");
    }

    SECTION("C++ new") {
        engine_->globalThis().set("getDerived", qjspp::Function{[](qjspp::Arguments const& args) -> qjspp::Value {
                                      auto engine = args.engine();
                                      auto der    = engine->newInstanceOfRaw(DerivedDefine, new Derived{888});
                                      return der;
                                  }});
        auto der = engine_->eval("getDerived()");
        REQUIRE(der.isObject());
        REQUIRE(engine_->eval("getDerived().derivedMember").asNumber().getInt32() == 888);
    }

    SECTION("JavaSceipt inherit") {
        REQUIRE_NOTHROW(engine_->eval(R"(
            class MyDerived extends Derived {
                constructor() {
                    super(123456);
                }
            };
            const my = new MyDerived();
            debug(`baseMember: ${my.baseMember}`);
            debug(`baseBar: ${my.baseBar()}`);
        )"));
    }

    SECTION("$equals") {
        REQUIRE_NOTHROW(engine_->eval(R"(
            let lhs = new Derived(1234);
            let rhs = new Derived(5678);
            globalThis.val = lhs.$equals(rhs);
        )"));
        REQUIRE(engine_->globalThis().has("val") == true);
        REQUIRE(engine_->globalThis().get("val").asBoolean().value() == false);
    }
}

qjspp::bind::meta::ModuleDefine NativeModuleDef =
    qjspp::defineModule("native").addClass(UtilDefine).addClass(BaseDefine).addClass(DerivedDefine).build();

TEST_CASE_METHOD(TestEngineFixture, "Module Binding") {
    qjspp::Locker scope{engine_};

    engine_->registerModule(NativeModuleDef);

    REQUIRE_NOTHROW(
        engine_->eval("import { Base } from 'native'; Base.baseTrue();", "<eval>", qjspp::JsEngine::EvalType::kModule)
    );

    REQUIRE_NOTHROW(
        engine_->eval("import { Util } from 'native'; Util.add(8,8);", "<eval>", qjspp::JsEngine::EvalType::kModule)
    );

    REQUIRE_NOTHROW(engine_->loadScript(std::filesystem::current_path() / "tests" / "module.js"));
}


class TestForm {
public:
    using Callback = std::function<void(int)>;
    Callback cb_;

    TestForm() = default;

    void setCallback(Callback cb) { cb_ = std::move(cb); }

    void call(int val) {
        if (cb_) {
            cb_(val);
        }
    }
};


qjspp::Value JsAssert(qjspp::Arguments const& args) {
    REQUIRE((args.length() == 1 || args.length() == 2));
    REQUIRE(args[0].isBoolean());
    bool val = args[0].asBoolean().value() == true;
    if (args.length() == 2 && !val) {
        std::cout << "Assert Failed: " << args[1].toString().value() << std::endl;
    }
    REQUIRE(val);
    return {};
}

qjspp::bind::meta::ClassDefine TestFormDefine = qjspp::bind::defineClass<TestForm>("TestForm")
                                                    .constructor<>()
                                                    .instanceMethod("setCallback", &TestForm::setCallback)
                                                    .instanceMethod("call", &TestForm::call)
                                                    .build();

TEST_CASE_METHOD(TestEngineFixture, "Test Callback") {
    qjspp::Locker scope{engine_};

    engine_->registerClass(TestFormDefine);
    engine_->globalThis().set("assert", qjspp::Function{&JsAssert});

    engine_->eval(R"(
        let fm = new TestForm();
        fm.setCallback((val) => {
            assert(val == 114514);
        });
        fm.call(114514);
    )");
}


class AbstractFoo {
public:
    AbstractFoo()          = default;
    virtual ~AbstractFoo() = default;

    virtual std::string foo() = 0;
};

qjspp::bind::meta::ClassDefine AbstractFooDef = qjspp::bind::defineClass<AbstractFoo>("AbstractFoo")
                                                    .disableConstructor()
                                                    .instanceMethod("foo", &AbstractFoo::foo)
                                                    .build();

class FooImpl : public AbstractFoo {
public:
    std::string foo() override { return "foo"; }
};

TEST_CASE_METHOD(TestEngineFixture, "Abstract Class") {
    qjspp::Locker scope{engine_};

    auto impl = std::make_shared<FooImpl>();

    engine_->registerClass(AbstractFooDef);
    engine_->globalThis().set("assert", qjspp::Function{&JsAssert});
    engine_->globalThis().set("getAbstractFoo", qjspp::Function{[impl](qjspp::Arguments const& args) -> qjspp::Value {
                                  REQUIRE(args.length() == 0);
                                  auto i = impl;
                                  return args.engine()->newInstanceOfShared(AbstractFooDef, std::move(i));
                              }});

    REQUIRE_NOTHROW(engine_->eval(R"(
        let foo = getAbstractFoo();
        assert(foo.foo() == "foo");
    )"));
}


class Builder {
public:
    std::ostringstream str_;

    Builder() = default;

    Builder& append(std::string const& str) {
        str_ << str;
        return *this;
    }

    std::string build() const { return str_.str(); }
};

qjspp::bind::meta::ClassDefine BuilderDefine = qjspp::bind::defineClass<Builder>("Builder")
                                                   .constructor<>()
                                                   .instanceMethod("append", &Builder::append)
                                                   .instanceMethod("build", &Builder::build)
                                                   .build();

TEST_CASE_METHOD(TestEngineFixture, "Builder Pattern") {
    qjspp::Locker scope{engine_};

    engine_->registerClass(BuilderDefine);
    engine_->globalThis().set("assert", qjspp::Function{&JsAssert});

    REQUIRE_NOTHROW(engine_->eval(R"(
        let builder = new Builder();
        let str = builder.append("Hello").append(" World").build();
        assert(str == "Hello World");
    )"));
}


// enum bind

enum class Color {
    Red = 0,
    Green,
    Blue,
};

qjspp::bind::meta::EnumDefine ColorDef_ = qjspp::bind::defineEnum<Color>("Color")
                                              .value("Red", Color::Red)
                                              .value("Green", Color::Green)
                                              .value("Blue", Color::Blue)
                                              .build();

TEST_CASE_METHOD(TestEngineFixture, "Enum Bind") {
    qjspp::Locker scope{engine_};

    engine_->registerEnum(ColorDef_);

    REQUIRE(engine_->eval("Color.$name").asString().value() == "Color");
    REQUIRE(engine_->eval("Color.Red").asNumber().getInt32() == 0);
    REQUIRE(engine_->eval("Color.Green").asNumber().getInt32() == 1);
    REQUIRE(engine_->eval("Color.Blue").asNumber().getInt32() == 2);
}

qjspp::bind::meta::ModuleDefine ColorModuleDef_ = qjspp::defineModule("Color").addEnum(ColorDef_).build();

TEST_CASE_METHOD(TestEngineFixture, "Enum Module Bind") {
    qjspp::Locker scope{engine_};

    engine_->registerModule(ColorModuleDef_);
    engine_->globalThis().set("assert", qjspp::Function{&JsAssert});

    REQUIRE_NOTHROW(engine_->eval(
        R"(
            import { Color } from "Color";
            assert(Color.$name == "Color");
            assert(Color.Red == 0);
            assert(Color.Green == 1);
            assert(Color.Blue == 2);
        )",
        "<eval>",
        qjspp::JsEngine::EvalType::kModule
    ));
}


// toStringTag

TEST_CASE_METHOD(TestEngineFixture, "toStringTag") {
    qjspp::Locker scope{engine_};

    engine_->registerEnum(ColorDef_);

    REQUIRE(engine_->eval("Color.toString()").asString().value() == "[object Color]");
}


// overload constructor
class PointMeta {
public:
    int  x_{0}, y_{0};
    bool external_ = false;

    PointMeta() = default;
    PointMeta(int x, int y) : x_(x), y_(y) {}
    PointMeta(int x, int y, bool external) : x_(x), y_(y), external_(external) {}
};
auto ScriptPointMeta = qjspp::bind::defineClass<PointMeta>("PointMeta")
                           .constructor<>()
                           .constructor<int, int>()
                           .constructor<int, int, bool>()
                           .instanceProperty("x", &PointMeta::x_)
                           .instanceProperty("y", &PointMeta::y_)
                           .instanceProperty("external", &PointMeta::external_)
                           .build();

TEST_CASE_METHOD(TestEngineFixture, "Overload Constructor") {
    qjspp::Locker scope{engine_};

    engine_->registerClass(ScriptPointMeta);
    engine_->globalThis().set("assert", qjspp::Function{&JsAssert});

    REQUIRE_NOTHROW(engine_->eval(R"(
        let p = new PointMeta();
        assert(p.x == 0);
        assert(p.y == 0);
        assert(p.external == false);
    )"));
    REQUIRE_NOTHROW(engine_->eval(R"(
        let p2 = new PointMeta(1, 2);
        assert(p2.x == 1);
        assert(p2.y == 2);
        assert(p2.external == false);
    )"));
    REQUIRE_NOTHROW(engine_->eval(R"(
        let p3 = new PointMeta(1, 2, true);
        assert(p3.x == 1);
        assert(p3.y == 2);
        assert(p3.external == true);
    )"));
    REQUIRE_THROWS_MATCHES(
        engine_->eval("new PointMeta(1, 2, 3, 4)"),
        qjspp::JsException,
        Catch::Matchers::Message("This native class cannot be constructed.")
    );
}


// property: Non-value type, reference mechanism
class Vec3 {
public:
    float x, y, z;

    Vec3() = default;
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}

    std::string toString() { return std::format("Vec3({},{},{})", x, y, z); }
};
class AABB {
public:
    Vec3 min, max;

    AABB() = default;
    AABB(const Vec3& min, const Vec3& max) : min(min), max(max) {}
};

auto ScriptVec3 = qjspp::bind::defineClass<Vec3>("Vec3")
                      .constructor<>()
                      .constructor<float, float, float>()
                      .instanceProperty("x", &Vec3::x)
                      .instanceProperty("y", &Vec3::y)
                      .instanceProperty("z", &Vec3::z)
                      .instanceMethod("toString", &Vec3::toString)
                      .build();


namespace qjspp::bind {
template <>
struct TypeConverter<Vec3> {
    static Value toJs(Vec3 const& ref) {
        return Locker::currentEngineChecked().newInstanceOfRaw(ScriptVec3, const_cast<Vec3*>(&ref));
    }
    static Vec3* toCpp(Value const& val) {
        return Locker::currentEngineChecked().getNativeInstanceOf<Vec3>(val.asObject(), ScriptVec3);
    }
};
} // namespace qjspp::bind


auto ScriptAABB = qjspp::bind::defineClass<AABB>("AABB")
                      .constructor<>()
                      .constructor<const Vec3&, const Vec3&>()
                      .instancePropertyRef("min", &AABB::min, ScriptVec3)
                      .instancePropertyRef("max", &AABB::max, ScriptVec3)
                      .build();

TEST_CASE_METHOD(TestEngineFixture, "Non-value type ref") {
    qjspp::Locker scope{engine_};

    engine_->registerClass(ScriptVec3);
    engine_->registerClass(ScriptAABB);
    engine_->globalThis().set("assert", qjspp::Function{&JsAssert});

    REQUIRE_NOTHROW(engine_->eval(R"(
        let aabb = new AABB(new Vec3(0, 0, 0), new Vec3(1, 1, 1));
        let min = aabb.min;
        min.x = 2;
        assert(aabb.min.x === min.x) // min is a reference to aabb.min
    )"));

    REQUIRE_NOTHROW(engine_->eval(R"(
        let ab = new AABB();
        let mm = ab.min;
        assert(ab.min.$equals(mm), `${ab.min}/${mm}`);

        ab.min = new Vec3(1, 2, 3);
        assert(ab.min.$equals(mm), `${ab.min}/${mm}`);
    )"));
}