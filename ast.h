#ifndef HIDL_HIDL_LANGUAGE_H_
#define HIDL_HIDL_LANGUAGE_H_

#include <memory>
#include <string>
#include <vector>
#include <map>
#include "logging.h"
#include "options.h"

//#include <android-base/macros.h>
//#include <android-base/strings.h>

#include <io_delegate.h>
using std::string;
using std::vector;
using std::pair;

struct yy_buffer_state;
typedef yy_buffer_state* YY_BUFFER_STATE;

class Parser;

using Subs = vector<pair<string, string>>;

class Thing {
 public:
  Thing();
  Thing(string text);
  const string& GetText() const { return text_; }
  void SetText(string text) { text_ = text; }
  const char *c_str() const { return text_.c_str(); }
  const string& GetComments() const { return comments_; }
  virtual const string TypeName() = 0;

  virtual void Dump();
  virtual string Generate(string /*section*/) { return ""; }
  static void SetParser(Parser *ps);
  virtual const Subs GetSubs(string /*section*/) const { Subs subs; return subs; }
 protected:
  static Parser *ps_; // At least it's not global :-)
  string text_;
  string comments_;
};

class Header : public Thing {
 public:
  void Dump() override;
  string Generate(string section) override;
  const string TypeName() override { return "header"; };
};

class Element : public Thing {
 public:
  const string& GetComments() const { return comments_; }
  virtual long GetScalarValue() const {CHECK(false); return false;}
  virtual bool HasIntegerValue() const { return false; }
  virtual bool HasScalarValue() const { return false; }
  virtual bool IsDefinedType() const { return false; }
  virtual long GetIntegerValue() const;
  virtual string NoQuoteText() const { return GetText();}
  virtual bool HasStringValue() const { return false; }
  void AddDottedElement(Element *element);
  int Line() const { return line_; }
  void Dump() override;
  string Generate(string section) override;
  const string TypeName() override { return "element"; }
  virtual const string ElementTypename() const = 0;

 protected:
  Element(const string& text, const string& comments, int Line);
  Element(const string& text, int Line);

 private:
  string comments_;
  int line_;
  DISALLOW_COPY_AND_ASSIGN(Element);
};

class CharElement : public Element {
 public:
  CharElement(const string& text, const string& comments, int Line);
  CharElement(const string& text, int Line);
  virtual const string ElementTypename() const override { return "char"; }
  virtual bool HasScalarValue() const { return true; }
  DISALLOW_COPY_AND_ASSIGN(CharElement);
};

class IntElement : public Element {
 public:
  IntElement(const string& text, const string& comments, int Line);
  //  IntElement(const string& text, int Line);
  virtual const string ElementTypename() const override { return "int"; }
  virtual bool HasScalarValue() const { return true; }
  virtual bool HasIntegerValue() const { return true; }
  virtual long GetIntegerValue() const override { return stoi(text_); }
  DISALLOW_COPY_AND_ASSIGN(IntElement);
};

class NameElement : public Element {
 public:
  NameElement(const string& text, const string& comments, int Line);
  //  NameElement(const string& text, int Line);
  virtual const string ElementTypename() const override { return "name"; }
  virtual bool HasScalarValue() const override;  // need to look up name
  virtual bool HasIntegerValue() const override;
  virtual long GetScalarValue() const override;  // need to look up name
  virtual long GetIntegerValue() const override;
  virtual bool IsDefinedType() const override;
  DISALLOW_COPY_AND_ASSIGN(NameElement);
};

class StringElement : public Element {
 public:
  StringElement(const string& text, const string& comments, int Line);
  StringElement(const string& text, int Line);
  virtual const string ElementTypename() const override { return "string"; }
  virtual string NoQuoteText() const override { return GetText().substr(1,GetText().length()-2); }
  virtual bool HasStringValue() const { return true; }

  DISALLOW_COPY_AND_ASSIGN(StringElement);
};

class CommentElement : public Element {
 public:
  CommentElement(const string& text, const string& comments, int Line);
  //  CommentElement(const string& text, int Line);
  virtual const string ElementTypename() const override { return "comment"; }

  DISALLOW_COPY_AND_ASSIGN(CommentElement);
};

class ErrorElement : public Element {
 public:
  ErrorElement(const string& text, const string& comments, int Line);
  //  ErrorElement(const string& text, int Line);
  virtual const string ElementTypename() const override { return "error"; }

  DISALLOW_COPY_AND_ASSIGN(ErrorElement);
};


class Const : public Thing {
 public:
  Const(Element *name, Element *value);
  const Element *GetName() const { return name_; }
  const Element *GetValue() const { return value_; }
  void Dump() override;
  string Generate(string ) override;
  const Subs GetSubs(string section) const override;
  const string TypeName() override { return "const"; }

 private:
  Element *name_;
  Element *value_;
  DISALLOW_COPY_AND_ASSIGN(Const);
};

class Field;
class Type;

// TODO - split Fields into TypedFields and EnumFields
class Fields {
 public:
  void Add(Field *field) { fields_.push_back(field); }
  string GenCommaList(string section, string prev, bool out_params);
  string GenCommaList(string section, string prev) {return GenCommaList(section, prev, false);}
  string GenCommaList(string section) {return GenCommaList(section, "", false);}
  string GenCommaNameList(string section, string prev_list, string snippet = "");

  string GenSemiList(string section);
  string GenByType(string section, string prefix);
  string TextByPrefix(string section, string prefix);
  string GenVtsList(string section, string label);
  int Size() { return fields_.size(); }
  void Dump();
  bool HasPtrFixup();
  bool HasFdFixup();
  vector<Field *> GetVec() { return fields_; }

 private:
  vector<Field *> fields_;
};

class Type : public Thing {
 public:
  Type() = default;

  virtual bool IsVec() { return false; }
  virtual bool IsRef() { return false; }
  virtual bool IsArray() { return false; }
  virtual bool IsHandle() { return false; }
  virtual bool IsString() { return false; }
  virtual bool IsCompound() { return false; }
  virtual bool IsPrimitive() { return false; }
  virtual Fields *GetFields() { return(empty_fields_); }
  virtual bool HasFdFixup() { return false; }
  virtual bool HasPtrFixup() { return false; }
  virtual string VtsType() const { return "VtsTypeUnknown"; }
  virtual string SubtypeSuffix() { return ""; }
  virtual string TypeOfEnum(string /*section*/) { return "Error, not enum type"; }
  bool HasFixup();

 private:
  static Fields *empty_fields_;
  DISALLOW_COPY_AND_ASSIGN(Type);
};

class CompoundType : public Type {
 public:
  CompoundType(Fields *fields);
  bool IsCompound() override { return true; }
  Fields *GetFields() override { return fields_; }
  virtual bool HasPtrFixup() override { return fields_->HasPtrFixup(); }
  virtual bool HasFdFixup() override { return fields_->HasFdFixup(); }

 protected:
  Fields *fields_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CompoundType);
};

class StructType : public CompoundType {
 public:
  StructType(Fields *fields);
  void Dump() override;
  const string TypeName() override { return "struct_type"; }
  string Generate(string prefix) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(StructType);
};

class EnumType : public CompoundType {
 public:
  EnumType(Type* type, Fields* fields);
  void Dump() override;
  const string TypeName() override { return "enum_type"; }
  string Generate(string prefix) override;
  string TypeOfEnum(string section) override { return type_->Generate(section); }

 private:
  Type *type_;

  DISALLOW_COPY_AND_ASSIGN(EnumType);
};

class UnionType : public CompoundType {
 public:
  UnionType(Fields *fields); // Undiscriminated
  UnionType(Type* type, Fields* fields); // Disc
  void Dump() override;
  const string TypeName() override { return "union_type"; }
  string Generate(string prefix) override;

 private:
  Type *discriminator_;

  DISALLOW_COPY_AND_ASSIGN(UnionType);
};

class DerivedType : public Type {
 public:
  DerivedType(Type *base);
  DerivedType() = default;
  virtual bool HasFdFixup() override { return base_->HasFdFixup(); }
  virtual bool HasPtrFixup() override { return base_->HasPtrFixup(); }
  virtual Type *GetBase() { return base_; }

 protected:
  Type *base_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DerivedType);
};

class VecType : public DerivedType {
 public:
  VecType(Type *type);
  void Dump() override;
  const Subs GetSubs(string section) const override;
  bool IsVec() override { return true; }
  const string TypeName() override { return "vec"; }
  string Generate(string prefix) override;
  virtual bool HasPtrFixup() override { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(VecType);
};

class RefType : public DerivedType {
 public:
  RefType(Type *type);
  void Dump() override;
  bool IsRef() override { return true; }
  const Subs GetSubs(string section) const override;
  const string TypeName() override { return "ref"; }
  string Generate(string section) override;
  virtual bool HasFdFixup() override { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(RefType);
};

class ArrayType : public DerivedType {
 public:
  ArrayType(Type *type, Element *dimension);
  void Dump() override;
  bool IsArray() override { return true; }
  const Subs GetSubs(string section) const override;
  const string TypeName() override { return "array"; }
  string Generate(string section) override;
  string GenerateDimension() { return dimension_->GetText(); }

 private:
  Element *dimension_;

  DISALLOW_COPY_AND_ASSIGN(ArrayType);
};

class NamedType : public DerivedType {
 public:
  NamedType(Element *name);
  Element *GetName() { return name_; }
  void Dump() override;
  const Subs GetSubs(string section) const override;
  const string TypeName() override { return "named_type"; }
  string Generate(string section) override;
  virtual string SubtypeSuffix() override;

 private:
  Element *name_;
  DISALLOW_COPY_AND_ASSIGN(NamedType);
};

class ScalarType : public Type {
 public:
  ScalarType(Element *name);
  virtual bool IsPrimitive() override { return true; }
  void Dump() override;
  const string TypeName() override { return "scalar"; }
  string Generate(string section) override;
  const Subs GetSubs(string section) const override;
  virtual bool HasPtrFixup() override { return false; }
  virtual bool HasFdFixup() override { return false; }
  virtual string VtsType() const override { return name_->GetText(); }
  virtual string SubtypeSuffix() override { return "_" + name_->GetText(); }
 private:
  Element *name_;
  DISALLOW_COPY_AND_ASSIGN(ScalarType);
};

class HandleType : public Type {
 public:
  HandleType();
  void Dump() override;
  bool IsHandle() override { return true; }
  const string TypeName() override { return "handle"; }
  string Generate(string section) override;
  virtual bool HasPtrFixup() override { return true; }
  virtual bool HasFdFixup() override { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(HandleType);
};

class OpaqueType : public Type {
 public:
  OpaqueType();
  void Dump() override;
  const string TypeName() override { return "opaque"; }
  string Generate(string section) override;
  virtual bool HasPtrFixup() override { return false; }
  virtual bool HasFdFixup() override { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(OpaqueType);
};

class StringType : public Type {
 public:
  StringType();
  void Dump() override;
  bool IsString() override { return true; }
  const string TypeName() override { return "string"; }
  string Generate(string section) override;
  virtual bool HasPtrFixup() override { return true; }
  virtual bool HasFdFixup() override { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(StringType);
};

class TypeDecl : public DerivedType {
 public:
  TypeDecl(Element *name, Type *base);
  const Element *GetName() const { return name_; };

 protected:
  Element *name_;
 private:
  DISALLOW_COPY_AND_ASSIGN(TypeDecl);
};

class TypedefDecl : public TypeDecl {
 public:
  TypedefDecl(Element *name, Type *type);
  void Dump() override;
  const string TypeName() override { return "typedef_decl"; }
  string Generate(string section) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TypedefDecl);
};

class EnumDecl : public TypeDecl {
 public:
  EnumDecl(Element *name, Type *type);
  void Dump() override;
  const string TypeName() override { return "enum_decl"; }
  const Subs GetSubs(string section) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(EnumDecl);
};

class StructDecl : public TypeDecl {
 public:
  StructDecl(Element *name, Type *type);
  void Dump() override;
  const string TypeName() override { return "struct_decl"; }
  const Subs GetSubs(string section) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(StructDecl);
};

class UnionDecl : public TypeDecl {
 public:
  UnionDecl(Element *name, Type *type);
  void Dump() override;
  const string TypeName() override { return "union_decl"; }
  string Generate(string section) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UnionDecl);
};

class Annotation;

class Field : public Thing {
 public:
  void Dump() override;
  int Line() { return name_->Line(); }
  string Generate(string section) override;
  virtual Type *GetType() const { return nullptr; }
  const string TypeName() override { return "field"; }
  Element *GetName() { return name_; }
  virtual Element *GetValue() { return nullptr; }
  const string& GetComments() const { return comments_; }
  const Subs GetSubs(string section) const override;
  const string NameText() { return name_->GetText(); }
  void SetAnnotation(Annotation *a) { annotation_ = a; }
  string GenVtsValues(string section);
  virtual string GetInitText();

 protected:
  virtual void BuildText() = 0;
  Field(Element *name, vector<Element*>* selectors);
  Field(Element *name, Element *selected);
  Field(Element *name);

  Annotation *annotation_ = nullptr;
  string comments_;
  Element *initializer_ = nullptr;
  Element *name_ = nullptr;
  Element *selected_ = nullptr;
  std::vector<Element*> *selectors_;
  DISALLOW_COPY_AND_ASSIGN(Field);
};

class TypedField : public Field {
 public:
  TypedField(Type *type, Element* name,
        std::vector<Element*>* selectors);
  TypedField(Type *type, Element *name); // Basic
  TypedField(Type *type, Element *name, Element *selected);

  virtual Type *GetType() const override { return type_; }

 private:
  virtual void BuildText() override;

  Type *type_;
  DISALLOW_COPY_AND_ASSIGN(TypedField);
};

class EnumField : public Field {
 public:
  EnumField(Element *name);

 private:
  virtual void BuildText() override;

  DISALLOW_COPY_AND_ASSIGN(EnumField);
};

class EnumValueField : public EnumField {
 public:
  EnumValueField(Element *name, Element *value);
  virtual Element *GetValue() override { return value_; }
 private:
  virtual void BuildText() override;

  Element *value_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(EnumValueField);
};

class Annotation;

class AnnotationValue : public Thing {
 public:
  AnnotationValue(Element *value);
  AnnotationValue(Annotation *annotation);
  const string TypeName() override { return "AnnotationValue"; };
  Element *GetValue() { return value_; }
  Annotation *GetAnnotation() { return annotation_; }
  string NoQuoteText();

 private:
  Element *value_ = nullptr;
  Annotation *annotation_ = nullptr;
  DISALLOW_COPY_AND_ASSIGN(AnnotationValue);
};

using AnnotationValues = std::vector<AnnotationValue*>;
using AnnotationEntry = std::pair<const string, AnnotationValues*>;
using AnnotationEntries = std::map<string, AnnotationValues*>;

class Annotation : public Thing {
 public:
  Annotation(Element *name);
  Annotation(Element *name, AnnotationValues *values);
  Annotation(Element *name,
             AnnotationEntries *entries);
  const string TypeName() override { return "Annotation"; };
  const string NameText() { return name_->GetText(); }
  int Line() { return name_->Line(); }
  AnnotationEntries *Entries() { return entries_; }
  bool HasKey(string key);
  AnnotationValues *GetValues(string key);
  AnnotationValues *GetUnnamedValues() { return values_; }

 private:
  AnnotationEntries *entries_ = nullptr;
  Element *name_;
  AnnotationValues *values_ = nullptr;
  DISALLOW_COPY_AND_ASSIGN(Annotation);
};

class Annotations : public Thing {
 public:
  Annotations() = default;
  void Add(Annotation *a);
  void Dump();
  const string TypeName() override { return "Annotations"; };
  const vector<Annotation*>GetAs() { return annotations_; }
  Annotation *GetAnnotation(string key);
  bool HasKey(string key);
  string GenVtsCalls(string section, string anno_name, string out_label);

 private:
  std::vector<Annotation*>annotations_;
  DISALLOW_COPY_AND_ASSIGN(Annotations);
};

class Function : public Thing {
 public:
  Function(Annotations* annotations,
           bool oneway,
           Element* name,
           Fields *fields,
           Fields *generates);
  void Dump() override;
  virtual const Subs GetSubs(string section) const override;
  const string TypeName() override { return "function"; }
  const Element *GetName() { return name_; }
  string GenCallflow(string section) const;

 private:
  Annotations* annotations_;
  Fields *fields_;
  Fields *generates_;
  Element* name_;
  bool oneway_;
  DISALLOW_COPY_AND_ASSIGN(Function);
};

using TypeMap = std::map<string, TypeDecl*>;

class Parser {
 public:
  explicit Parser(const android::hidl::IoDelegate& io_delegate,
                  std::string out_type);
  ~Parser();

  // Parse contents of file |filename|.
  bool ParseFile(const string& filename);

  bool FoundNoErrors() const { return error_ == 0; }
  const string& FileName() const { return filename_; }
  void* Scanner() const { return scanner_; }

  std::vector<string> Package() const;

  void AddComment(const string& comment);

  void SetInterface(Annotations *annotations, Element *name);
  Element *GetInterface() { return interface_; }
  string GetPackageName() const { return interface_->GetText(); }

  void SetVersion(int major, int minor);

  void SetNamespace(std::vector<Element *>* names);

  void AddImport(const std::vector<Element *>* names);

  void AddFunction(Function *function);

  void AddStruct(StructDecl *structure);

  void AddConst(Const *constant);

  void AddComment(Element *comment);

  void AddUnion(UnionDecl *type);

  void AddEnum(EnumDecl *en);

  void AddTypedef(TypedefDecl* type);

  bool IsTypeDeclared(Element *name);

  TypeDecl *GetDeclaredType(Element *name); // nullptr if not found

  void Error(const char *format, ...);

  void Error(int Line, const char *format, ...);

  void Error(string &s);
  int GetErrorCount() { return error_; }

  void Dump();
  void Write();
  void SetWriter(android::hidl::CodeWriterPtr writer);
  string TextByPrefix(string section, string prefix);
  string CallEnumList(string section);
  string CallbackDeclList(string section);
  void BuildNamespaceText(string section,
                          std::vector<Element *>*namespace_,
                          string& namespace_open,
                          string& namespace_close,
                          string& namespace_slashes);
  void WriteDepFileIfNeeded(
          std::unique_ptr<android::hidl::CppOptions> options,
          android::hidl::IoDelegate &io_delegate);

 private:
  void AddThing(Thing *thing);
  void RegisterType(TypeDecl *decl);
  void RegisterConstant(const Element *name, const Element *value);
  void Validate(); // Will call Error() if any are found
  void VaError(const char *format, va_list args);

  const android::hidl::IoDelegate& io_delegate_;
  std::string section_;
  int error_ = 0;
  string filename_;
  void* scanner_ = nullptr;
  android::hidl::CodeWriterPtr writer_;
  std::unique_ptr<string> raw_buffer_;
  YY_BUFFER_STATE buffer_;
  bool version_set_ = false;
  int version_major_, version_minor_;
  std::vector<Element *>* namespace_ = nullptr;
  Element *interface_ = nullptr;
  Annotations *interface_annotations_ = nullptr;
  TypeMap types_;
  std::map<string, const Element*> consts_;
  Fields vars_;
  std::vector<const std::vector<Element *>*> imports_;
  std::vector<Thing *> things_;
  DISALLOW_COPY_AND_ASSIGN(Parser);
};

#endif // HIDL_HIDL_LANGUAGE_H_
