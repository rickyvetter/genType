open GenTypeCommon;

let createBucklescriptBlock = "CreateBucklescriptBlock" ++ ".__";

type recordGen = {
  mutable unboxed: int,
  mutable boxed: int,
};

type recordValue = int;

type moduleItemGen = {mutable itemValue: int};

type moduleItem = int;

let blockTagValue = (~config, i) =>
  string_of_int(i) ++ (config.language == TypeScript ? " as any" : "");

let emitRecordAsInt = (~config, recordValue) =>
  recordValue |> blockTagValue(~config);

let recordValueToString = recordValue => recordValue |> string_of_int;

let recordGen = () => {unboxed: 0, boxed: 0};

let newRecordValue = (~unboxed, recordGen) =>
  if (unboxed) {
    let v = recordGen.unboxed;
    recordGen.unboxed = recordGen.unboxed + 1;
    v;
  } else {
    let v = recordGen.boxed;
    recordGen.boxed = recordGen.boxed + 1;
    v;
  };

let moduleItemGen = () => {itemValue: 0};

let newModuleItem = moduleItemGen => {
  let v = moduleItemGen.itemValue;
  moduleItemGen.itemValue = moduleItemGen.itemValue + 1;
  v;
};

let emitModuleItem = itemValue => itemValue |> string_of_int;

let emitVariantLabel = (~comment=true, ~polymorphic, label) =>
  if (polymorphic) {
    (comment ? label |> EmitText.comment : "")
    ++ (label |> Btype.hash_variant |> string_of_int);
  } else {
    label;
  };

let emitVariantGetLabel = (~polymorphic, x) =>
  if (polymorphic) {
    x |> EmitText.arrayAccess(~index=0);
  } else {
    x |> EmitText.fieldAccess(~label="tag");
  };

let emitVariantGetPayload = (~numArgs, ~polymorphic, x) =>
  if (polymorphic) {
    x |> EmitText.arrayAccess(~index=1);
  } else if (numArgs == 1) {
    x |> EmitText.arrayAccess(~index=0);
  } else {
    /* to convert a runtime block to a tuple, remove the tag */
    x |> EmitText.arraySlice;
  };

let emitVariantWithPayload = (~config, ~label, ~numArgs, ~polymorphic, x) =>
  if (polymorphic) {
    EmitText.array([label |> emitVariantLabel(~polymorphic), x]);
  } else {
    config.emitCreateBucklescriptBlock = true;
    let args = numArgs == 1 ? [x] |> EmitText.array : x;
    createBucklescriptBlock |> EmitText.funCall(~args=[label, args]);
  };

let jsVariantTag = "tag";
let jsVariantValue = "value";

let emitJSVariantGetLabel = x =>
  x |> EmitText.fieldAccess(~label=jsVariantTag);

let emitJSVariantGetPayload = x =>
  x |> EmitText.fieldAccess(~label=jsVariantValue);

let emitJSVariantWithPayload = (~label, x) =>
  "{"
  ++ jsVariantTag
  ++ ":"
  ++ label
  ++ ", "
  ++ jsVariantValue
  ++ ":"
  ++ x
  ++ "}";

let isMutableObjectField = name =>
  String.length(name) >= 2
  && String.sub(name, String.length(name) - 2, 2) == "#=";

/* Mutable fields, i.e. fields annotated "[@bs.set]"
   are represented as extra fields called "fieldName#="
   preceding the normal field. */
let checkMutableObjectField = (~previousName, ~name) =>
  previousName == name ++ "#=";

let default = "$$default";

module Mangle = {
  let keywords = [|
    "and",
    "as",
    "assert",
    "begin",
    "class",
    "constraint",
    "do",
    "done",
    "downto",
    "else",
    "end",
    "exception",
    "external",
    "false",
    "for",
    "fun",
    "function",
    "functor",
    "if",
    "in",
    "include",
    "inherit",
    "initializer",
    "lazy",
    "let",
    "match",
    "method",
    "module",
    "mutable",
    "new",
    "nonrec",
    "object",
    "of",
    "open",
    "or",
    "private",
    "rec",
    "sig",
    "struct",
    "then",
    "to",
    "true",
    "try",
    "type",
    "val",
    "virtual",
    "when",
    "while",
    "with",
    "mod",
    "land",
    "lor",
    "lxor",
    "lsl",
    "lsr",
    "asr",
  |];

  let table = Hashtbl.create(keywords |> Array.length);
  keywords |> Array.iter(x => Hashtbl.add(table, "_" ++ x, x));

  /*
     Apply bucklescript's marshaling rules for object field names:
     Remove trailing "__" if present.
     Otherwise remove leading "_" when followed by an uppercase letter, or keyword.
   */
  let translate = x => {
    let len = x |> String.length;
    if (len > 2 && x.[len - 1] == '_' && x.[len - 2] == '_') {
      /* "foo__" -> "foo" */
      String.sub(x, 0, len - 2);
    } else if (len > 1 && x.[0] == '_') {
      if (x.[1] >= 'A' && x.[1] <= 'Z') {
        /* "_Uppercase" => "Uppercase" */
        String.sub(x, 1, len - 1);
      } else {
        /* "_rec" -> "rec" */
        switch (Hashtbl.find(table, x)) {
        | y => y
        | exception Not_found => x
        };
      };
    } else {
      x;
    };
  };
};

let mangleObjectField = Mangle.translate;