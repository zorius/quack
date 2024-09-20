/*
 * Copyright (C) 2016 Square, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "JavaType.h"
#include "JString.h"
#include "JavaExceptions.h"

jvalue JavaType::callMethod(duk_context* ctx, JNIEnv *env, jmethodID methodId, jobject javaThis,
                            jvalue* args) const {
  jobject returnValue = env->CallObjectMethodA(javaThis, methodId, args);
  checkRethrowDuktapeError(env, ctx);
  jvalue result;
  result.l = returnValue;
  return result;
}

std::string getName(JNIEnv *env, jclass javaClass) {
  const jclass classClass = env->GetObjectClass(javaClass);
  const jmethodID method = env->GetMethodID(classClass, "getName", "()Ljava/lang/String;");
  const jobject jMethodName = env->CallObjectMethod(javaClass, method);
  const JString methodName(env, static_cast<jstring>(jMethodName));
  env->DeleteLocalRef(classClass);
  env->DeleteLocalRef(jMethodName);
  return methodName.str();
}

namespace  {

struct Void : public JavaType {
  Void(const GlobalRef& classRef, bool pushUndefined)
      : JavaType(classRef)
      , m_pushUndefined(pushUndefined) {
  }

  jvalue pop(duk_context* ctx, JNIEnv*, bool) const override {
    duk_pop(ctx);
    jvalue value;
    value.l = nullptr;
    return value;
  }

  duk_ret_t push(duk_context* ctx, JNIEnv*, const jvalue&) const override {
    if (m_pushUndefined) {
      duk_push_undefined(ctx);
      return 1;
    } else {
      return 0;
    }
  }

  jvalue callMethod(duk_context* ctx, JNIEnv* env, jmethodID methodId, jobject javaThis,
                    jvalue *args) const override {
    env->CallVoidMethodA(javaThis, methodId, args);
    checkRethrowDuktapeError(env, ctx);
    jvalue result;
    result.l = nullptr;
    return result;
  }

  const bool m_pushUndefined;
};

struct String : public JavaType {
  String(const GlobalRef& classRef)
      : JavaType(classRef) {
  }

  jvalue pop(duk_context* ctx, JNIEnv* env, bool inScript) const override {
    if (!inScript && !duk_is_string(ctx, -1) && !duk_is_null(ctx, -1)) {
      const auto message =
          std::string("Cannot convert return value ") + duk_safe_to_string(ctx, -1) + " to String";
      duk_pop(ctx);
      throw std::invalid_argument(message);
    }

    jvalue value;
    // Check if the caller passed in a null string.
    value.l = duk_get_type(ctx, -1) != DUK_TYPE_NULL
              ? env->NewStringUTF(duk_require_string(ctx, -1))
              : nullptr;
    duk_pop(ctx);
    return value;
  }

  duk_ret_t push(duk_context* ctx, JNIEnv* env, const jvalue& value) const override {
    if (value.l != nullptr) {
      const JString result(env, static_cast<jstring>(value.l));
      duk_push_string(ctx, result);
    } else {
      duk_push_null(ctx);
    }
    return 1;
  }
};

struct Primitive : public JavaType {
public:
  Primitive(const GlobalRef& classRef, const GlobalRef& boxedClassRef)
      : JavaType(classRef)
      , m_boxedClassRef(boxedClassRef) {
  }

  virtual const char* getUnboxSignature() const = 0;
  virtual const char* getUnboxMethodName() const = 0;
  virtual const char* getBoxSignature() const = 0;
  virtual const char* getBoxMethodName() const { return "valueOf"; }

  bool isPrimitive() const override { return true; }
  jclass boxedClass() const { return static_cast<jclass>(m_boxedClassRef.get()); }
  const GlobalRef& boxedClassRef() const { return m_boxedClassRef; }

private:
  const GlobalRef m_boxedClassRef;
};

struct Boolean : public Primitive {
  Boolean(const GlobalRef& classRef, const GlobalRef& boxedClassRef)
      : Primitive(classRef, boxedClassRef) {
  }

  jvalue pop(duk_context* ctx, JNIEnv*, bool inScript) const override {
    if (!inScript && !duk_is_boolean(ctx, -1)) {
      const auto message =
          std::string("Cannot convert return value ") + duk_safe_to_string(ctx, -1) + " to boolean";
      duk_pop(ctx);
      throw std::invalid_argument(message);
    }
    jvalue value;
    value.z = duk_require_boolean(ctx, -1);
    duk_pop(ctx);
    return value;
  }

  duk_ret_t push(duk_context* ctx, JNIEnv*, const jvalue& value) const override {
    duk_push_boolean(ctx, value.z == JNI_TRUE);
    return 1;
  }

  jvalue callMethod(duk_context* ctx, JNIEnv* env, jmethodID methodId, jobject javaThis,
                    jvalue *args) const override {
    jboolean returnValue = env->CallBooleanMethodA(javaThis, methodId, args);
    checkRethrowDuktapeError(env, ctx);
    jvalue result;
    result.z = returnValue;
    return result;
  }

  const char* getUnboxSignature() const override {
    return "()Z";
  }
  const char* getUnboxMethodName() const override {
    return "booleanValue";
  }
  const char* getBoxSignature() const override {
    return "(Z)Ljava/lang/Boolean;";
  }
};

struct Integer : public Primitive {
  Integer(const GlobalRef& classRef, const GlobalRef& boxedClassRef)
      : Primitive(classRef, boxedClassRef) {
  }

  jvalue pop(duk_context* ctx, JNIEnv*, bool inScript) const override {
    if (!inScript && !duk_is_number(ctx, -1)) {
      const auto message =
          std::string("Cannot convert return value ") + duk_safe_to_string(ctx, -1) + " to int";
      duk_pop(ctx);
      throw std::invalid_argument(message);
    }
    jvalue value;
    value.i = duk_require_int(ctx, -1);
    duk_pop(ctx);
    return value;
  }

  duk_ret_t push(duk_context* ctx, JNIEnv*, const jvalue& value) const override {
    duk_push_int(ctx, value.i);
    return 1;
  }

  jvalue callMethod(duk_context* ctx, JNIEnv* env, jmethodID methodId, jobject javaThis,
                    jvalue *args) const override {
    jint returnValue = env->CallIntMethodA(javaThis, methodId, args);
    checkRethrowDuktapeError(env, ctx);
    jvalue result;
    result.i = returnValue;
    return result;
  }

  const char* getUnboxSignature() const override {
    return "()I";
  }
  const char* getUnboxMethodName() const override {
    return "intValue";
  }
  const char* getBoxSignature() const override {
    return "(I)Ljava/lang/Integer;";
  }
  bool isInteger() const override {
    return true;
  }
};

struct Double : public Primitive {
  Double(const GlobalRef& classRef, const GlobalRef& boxedClassRef)
      : Primitive(classRef, boxedClassRef) {
  }

  jvalue pop(duk_context* ctx, JNIEnv*, bool inScript) const override {
    if (!inScript && !duk_is_number(ctx, -1)) {
      const auto message =
          std::string("Cannot convert return value ") + duk_safe_to_string(ctx, -1) + " to double";
      duk_pop(ctx);
      throw std::invalid_argument(message);
    }
    jvalue value;
    value.d = duk_require_number(ctx, -1);
    duk_pop(ctx);
    return value;
  }

  duk_ret_t push(duk_context* ctx, JNIEnv*, const jvalue& value) const override {
    duk_push_number(ctx, value.d);
    return 1;
  }

  jvalue callMethod(duk_context* ctx, JNIEnv* env, jmethodID methodId, jobject javaThis,
                    jvalue* args) const override {
    jdouble returnValue = env->CallDoubleMethodA(javaThis, methodId, args);
    checkRethrowDuktapeError(env, ctx);
    jvalue result;
    result.d = returnValue;
    return result;
  }

  const char* getUnboxSignature() const override {
    return "()D";
  }
  const char* getUnboxMethodName() const override {
    return "doubleValue";
  }
  const char* getBoxSignature() const override {
    return "(D)Ljava/lang/Double;";
  }
};

class BoxedPrimitive : public JavaType {
public:
  BoxedPrimitive(JNIEnv* env, const Primitive& primitive)
      : JavaType(primitive.boxedClassRef())
      , m_primitive(primitive)
      , m_unbox(env->GetMethodID(primitive.boxedClass(),
                                 primitive.getUnboxMethodName(),
                                 primitive.getUnboxSignature()))
      , m_box(env->GetStaticMethodID(primitive.boxedClass(),
                                     primitive.getBoxMethodName(),
                                     primitive.getBoxSignature())) {
  }

  jvalue pop(duk_context* ctx, JNIEnv* env, bool inScript) const override {
    jvalue value;
    if (duk_get_type(ctx, -1) != DUK_TYPE_NULL) {
      value = m_primitive.pop(ctx, env, inScript);
      value.l = env->CallStaticObjectMethodA(m_primitive.boxedClass(), m_box, &value);
      checkRethrowDuktapeError(env, ctx);
    } else {
      duk_pop(ctx);
      value.l = nullptr;
    }
    return value;
  }

  duk_ret_t push(duk_context* ctx, JNIEnv* env, const jvalue& value) const override {
    if (value.l != nullptr) {
      const jvalue unboxedValue = m_primitive.callMethod(ctx, env, m_unbox, value.l, nullptr);
      return m_primitive.push(ctx, env, unboxedValue);
    } else {
      duk_push_null(ctx);
      return 1;
    }
  }

  bool isInteger() const override {
    return m_primitive.isInteger();
  }

private:
  const Primitive& m_primitive;
  const jmethodID m_unbox;
  const jmethodID m_box;
};

struct Object : public JavaType {
  Object(const GlobalRef& classRef, const JavaType& boxedBoolean, const JavaType& boxedDouble,
         JavaTypeMap& typeMap)
      : JavaType(classRef)
      , m_boxedBoolean(boxedBoolean)
      , m_boxedDouble(boxedDouble)
      , m_typeMap(typeMap) {
  }

  jvalue pop(duk_context* ctx, JNIEnv* env, bool inScript) const override {
    jvalue value;
    switch (duk_get_type(ctx, -1)) {
      case DUK_TYPE_NULL:
      case DUK_TYPE_UNDEFINED:
        value.l = nullptr;
        duk_pop(ctx);
        break;

      case DUK_TYPE_BOOLEAN:
        value = m_boxedBoolean.pop(ctx, env, inScript);
        break;

      case DUK_TYPE_NUMBER:
        value = m_boxedDouble.pop(ctx, env, inScript);
        break;

      case DUK_TYPE_STRING:
        value.l = env->NewStringUTF(duk_get_string(ctx, -1));
        duk_pop(ctx);
        break;

      default:
        const auto message =
            std::string("Cannot marshal return value ") + duk_safe_to_string(ctx, -1) + " to Java";
        if (inScript) {
          duk_error(ctx, DUK_RET_TYPE_ERROR, message.c_str());
        }
        duk_pop(ctx);
        throw std::invalid_argument(message);
    }
    return value;
  }

  duk_ret_t push(duk_context* ctx, JNIEnv* env, const jvalue& value) const override {
    if (value.l == nullptr) {
      duk_push_null(ctx);
      return 1;
    }
    return m_typeMap.get(env, env->GetObjectClass(value.l))->push(ctx, env, value);
  }

  const JavaType& m_boxedBoolean;
  const JavaType& m_boxedDouble;
  JavaTypeMap& m_typeMap;
};

/**
 * Loads the (primitive) TYPE member of {@code boxedClassName}.
 * For example, given java/lang/Integer, this function will return int.class.
 */
jclass getPrimitiveType(JNIEnv* env, jclass boxedClass) {
  const jfieldID typeField = env->GetStaticFieldID(boxedClass, "TYPE", "Ljava/lang/Class;");
  return static_cast<jclass>(env->GetStaticObjectField(boxedClass, typeField));
}

/**
 * Adds type adapters for primitive and boxed versions of {@code name}, as well as arrays of those
 * types.
 */
template<typename JavaTypeT>
JavaType* addTypeAdapters(std::map<std::string, const JavaType *> &types, JNIEnv *env,
                          const char *name, const char* primitiveSignature) {
  const jclass theClass = env->FindClass(name);
  const jclass primitiveClass = getPrimitiveType(env, theClass);
  const auto javaType = new JavaTypeT(GlobalRef(env, primitiveClass), GlobalRef(env, theClass));
  types.emplace(std::make_pair(getName(env, primitiveClass), javaType));
  types.emplace(std::make_pair(primitiveSignature, new JavaTypeT(*javaType)));

  const auto boxedType = new BoxedPrimitive(env, *javaType);
  types.emplace(std::make_pair(getName(env, theClass), boxedType));

  return boxedType;
}

/** convert Ljava.lang.String; -> java.lang.String */
std::string dropLandSemicolon(const std::string& s) {
  if (s.empty() || s[0] != 'L') {
    return s;
  }
  return s.substr(1, s.length() - 2);
}

} // anonymous namespace

JavaTypeMap::~JavaTypeMap() {
  for (auto entry : m_types) {
    delete entry.second;
  }
}

const JavaType* JavaTypeMap::get(JNIEnv* env, jclass c) {
  return find(env, getName(env, c));
}

const JavaType* JavaTypeMap::getObjectType(JNIEnv* env) {
  find(env, "java.lang.Object");
  return m_ObjectType;
}

const JavaType* JavaTypeMap::find(JNIEnv* env, const std::string& name) {
  if (m_types.empty()) {
    // Load up the map with the types we support.
    const jclass voidClass = env->FindClass("java/lang/Void");
    const jclass vClass = getPrimitiveType(env, voidClass);
    m_types.emplace(std::make_pair(getName(env, vClass), new Void(GlobalRef(env, vClass), false)));
    m_types.emplace(std::make_pair(getName(env, voidClass),
                                   new Void(GlobalRef(env, voidClass), true)));

    const jclass stringClass = env->FindClass("java/lang/String");
    const auto stringType = new String(GlobalRef(env, stringClass));
    m_types.emplace(std::make_pair(getName(env, stringClass), stringType));

    const auto boxedBooleanType = addTypeAdapters<Boolean>(m_types, env, "java/lang/Boolean", "Z");
    const auto boxedDoubleType = addTypeAdapters<Double>(m_types, env, "java/lang/Double", "D");
    addTypeAdapters<Integer>(m_types, env, "java/lang/Integer", "I");

    const jclass objectClass = env->FindClass("java/lang/Object");
    m_ObjectType = new Object(GlobalRef(env, objectClass), *boxedBooleanType,
                                       *boxedDoubleType, *this);
  }

  const auto I = m_types.find(name);
  if (I != m_types.end()) {
    return I->second;
  }

  return nullptr;
}
