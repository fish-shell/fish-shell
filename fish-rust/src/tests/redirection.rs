use crate::io::{IoChain, IoClose, IoFd};
use crate::redirection::dup2_list_resolve_chain;
use std::sync::Arc;

#[test]
fn test_dup2s() {
    let mut chain = IoChain::new();
    chain.push(Arc::new(IoClose::new(17)));
    chain.push(Arc::new(IoFd::new(3, 19)));
    let list = dup2_list_resolve_chain(&chain);
    assert_eq!(list.get_actions().len(), 2);

    let act1 = list.get_actions()[0];
    assert_eq!(act1.src, 17);
    assert_eq!(act1.target, -1);

    let act2 = list.get_actions()[1];
    assert_eq!(act2.src, 19);
    assert_eq!(act2.target, 3);
}

#[test]
fn test_dup2s_fd_for_target_fd() {
    let mut chain = IoChain::new();
    // note io_fd_t params are backwards from dup2.
    chain.push(Arc::new(IoClose::new(10)));
    chain.push(Arc::new(IoFd::new(9, 10)));
    chain.push(Arc::new(IoFd::new(5, 8)));
    chain.push(Arc::new(IoFd::new(1, 4)));
    chain.push(Arc::new(IoFd::new(3, 5)));
    let list = dup2_list_resolve_chain(&chain);

    assert_eq!(list.fd_for_target_fd(3), 8);
    assert_eq!(list.fd_for_target_fd(5), 8);
    assert_eq!(list.fd_for_target_fd(8), 8);
    assert_eq!(list.fd_for_target_fd(1), 4);
    assert_eq!(list.fd_for_target_fd(4), 4);
    assert_eq!(list.fd_for_target_fd(100), 100);
    assert_eq!(list.fd_for_target_fd(0), 0);
    assert_eq!(list.fd_for_target_fd(-1), -1);
    assert_eq!(list.fd_for_target_fd(9), -1);
    assert_eq!(list.fd_for_target_fd(10), -1);
}
