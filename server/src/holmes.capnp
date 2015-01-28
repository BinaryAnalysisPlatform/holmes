@0xaaef86128cdda946;
interface Holmes {
  # Values
  struct Val {
    union {
      uint64 @0 :UInt64;
      string @1 :Text;
      blob   @2 :Data;
    }
  }

  struct HType {
    union {
      uint64 @0 :Void;
      string @1 :Void;
      blob   @2 :Void;
    }
  }

  # Variables
  using Var = UInt32;

  # Logical facts
  using PredName = Text;
  using PredId   = UInt64;
  struct Fact {
    predicate @0 :PredId;
    args      @1 :List(Val);
  }

  struct BodyExpr {
    union {
      unbound @0 :Void;
      var     @1 :Var;
      const   @2 :Val;
    }
  }

  struct BodyClause {
    predicate @0 :PredId;
    args      @1 :List(BodyExpr);
  }

  struct Rule {
    head @0 :BodyClause;
    body @1 :List(BodyClause);
  }

  # Register a predicate
  newPredicate @0 (predName :PredName,
                   argTypes :List(HType)) -> (predId :PredId);

  # Add a fact to the extensional database
  newFact @1 (fact :List(Fact));
  
  # Ask the server to search or expand the intensional database
  # searching for a set of facts that matches a body clause
  # Returns the list of satisfying assignments to the body clauses.
  deriveFact @2 (target :List(BodyClause)) -> (ctx :List(List(Val)));

  # Add a rule to expand the intentional database
  newRule @3 (rule :Rule) -> ();
}