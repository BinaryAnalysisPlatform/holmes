@0xaaef86128cdda946;
using Cxx = import "/capnp/c++.capnp";

$Cxx.namespace("holmes");

interface Holmes {
  # Dynamic value type for use in facts
  struct Val {
    union {
      stringVal @0 :Text;
      addrVal   @1 :UInt64;
      blobVal   @2 :Data;
      jsonVal   @3 :Text;
      listVal   @4 :List(Val);
    }
  }
  
  # Type of a dynamic variable
  struct HType {
    union {
      string @0 :Void;
      addr   @1 :Void;
      blob   @2 :Void;
      json   @3 :Void;
      list   @4 :HType;
   }
  }

  # Variables
  using Var = UInt32;

  # Logical facts
  using FactName = Text;
  struct Fact {
    factName @0 :FactName;
    args     @1 :List(Val);
  }

  struct Range {
    name @0 :Var;
    start :union {
      exact @1 :UInt64;
      var   @2 :Var;
    }
    length :union {
      exact @3 :UInt64;
      var   @4 :Var;
    }
  }

  # Argument restriction when searching
  struct TemplateVal {
    union {
      exactVal @0 :Val;  #Argument must have this exact value
      unbound  @1 :Void; #Argument is unrestricted
      bound    @2 :Var;  #Argument is bound to a var and must be consistent
      forall   @3 :Var;  #Argument will aggregate a list of all possibilities
      subBlob  @4 :Range;
    }
  }

  # FactTemplate to be used as a search query
  # Variables _must_ be used from 0+ sequentially.
  struct FactTemplate {
    factName @0 :FactName;
    args     @1 :List(TemplateVal);
  }
  
  # Callback provided by an analysis
  interface Analysis {
    analyze @0 (context :List(Val)) -> (derived :List(Fact));
  }

  # Assert a fact to the server
  set @0 (facts :List(Fact));
  
  # Ask the server to search for facts
  derive @1 (target :List(FactTemplate)) -> (ctx :List(List(Val)));
  
  # Register as an analysis
  analyzer @2 (name        :Text,
               premises    :List(FactTemplate),
	       analysis    :Analysis);

  # Register a fact type
  # If it's not present, inform the DAL
  # If it is present, check compatibility
  registerType @3 (factName :Text,
                   argTypes :List(HType)) -> (valid :Bool);
}
