use common::*;

#[test]
pub fn register_where_rule() {
  single(&|holmes : &mut Holmes| {
    holmes_exec!(holmes, {
      predicate!(test_pred(string, bytes, uint64));
      rule!(test_pred(("bar"), (vec![2u8,2u8]), x) <= test_pred(("foo"), [_], x), {
        let (42) = (42)})
    })
  })
}

#[test]
pub fn where_const() {
  single(&|holmes : &mut Holmes| {
    try!(holmes_exec!(holmes, {
      predicate!(test_pred(string, bytes, uint64));
      rule!(test_pred(("bar"), (vec![2u8,2u8]), x) <= test_pred(("foo"), [_], [_]), {
          let x = (42)
      });
      fact!(test_pred("foo", vec![0u8,1u8], 16))
    }));
    assert_eq!(query!(holmes, test_pred(("bar"), x, y)).unwrap(),
               vec![vec![vec![2u8,2u8].to_value(), 42.to_value()]]);
    Ok(())
  })
}

#[test]
pub fn where_plus_two() {
  single(&|holmes : &mut Holmes| {
    try!(holmes_exec!(holmes, {
      predicate!(test_pred(string, bytes, uint64));
      func!(let plus_two : uint64 -> uint64 = |v : &u64| {v + 2});
      rule!(test_pred(("bar"), (vec![2u8,2u8]), y) <= test_pred(("foo"), [_], x), {
        let y = {plus_two([x])}
      });
      fact!(test_pred("foo", vec![0u8,1u8], 16))
    }));
    assert_eq!(query!(holmes, test_pred(("bar"), x, y)).unwrap(),
               vec![vec![vec![2u8,2u8].to_value(), 18.to_value()]]);
    Ok(())
  })
}

#[test]
pub fn where_destructure() {
  single(&|holmes : &mut Holmes| {
    try!(holmes_exec!(holmes, {
      predicate!(test_pred(uint64, bytes, uint64));
      func!(let succs : uint64 -> (uint64, uint64) = |n : &u64| {
        (n + 1, n + 2)
      });
      rule!(test_pred(y, (vec![2u8,2u8]), z) <= test_pred((3), [_], x), {
        let y, z = {succs([x])}
      });
      fact!(test_pred(3, vec![0u8,1u8], 16))
    }));
    assert_eq!(query!(holmes, test_pred(y, (vec![2u8,2u8]), z)).unwrap(),
               vec![vec![17.to_value(), 18.to_value()]]);
    Ok(())
  })
}

#[test]
pub fn where_iter() {
  single(&|holmes : &mut Holmes| {
    try!(holmes_exec!(holmes, {
      predicate!(test_pred(uint64, bytes, uint64));
      func!(let succs : uint64 -> [uint64] = |n : &u64| {
        vec![n + 1, n + 2]
      });
      rule!(test_pred((4), (vec![2u8,2u8]), y) <= test_pred((3), [_], x), {
        let [y] = {succs([x])}
      });
      fact!(test_pred(3, vec![0u8,1u8], 16))
    }));
    assert_eq!(query!(holmes, test_pred([_], (vec![2u8,2u8]), x)).unwrap(),
               vec![vec![17.to_value()], vec![18.to_value()]]);
    Ok(())
  })
}
