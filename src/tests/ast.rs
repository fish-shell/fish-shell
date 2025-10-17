use crate::ast::{self, Node, is_same_node};
use crate::wchar::prelude::*;

const FISH_FUNC: &str = r#"
function stuff --description 'Stuff'
    set -l log "/tmp/chaos_log.(random)"
    set -x PATH /custom/bin $PATH

    echo "[$USER] Hooray" | tee -a $log 2>/dev/null

    time if test (count $argv) -eq 0
        echo "No targets specified" >> $log 2>&1
        return 1
    end

    for target in $argv
        command bash -c "echo" >> $log 2> /dev/null
        switch $status
            case 0
                echo "Success" | tee -a $log
            case '*'
                echo "Failure" >> $log
        end
    end
    set_color green
end
"#;

#[test]
fn test_is_same_node() {
    // is_same_node is pretty subtle! Let's check it.
    let src = WString::from_str(FISH_FUNC);
    let ast = ast::parse(&src, Default::default(), None);
    assert!(!ast.errored());
    let all_nodes: Vec<&dyn Node> = ast.walk().collect();
    for i in 0..all_nodes.len() {
        for j in 0..all_nodes.len() {
            let same = is_same_node(all_nodes[i], all_nodes[j]);
            if i == j {
                assert!(same, "Node {} should be the same as itself", i);
            } else {
                assert!(!same, "Node {} should not be the same as node {}", i, j);
            }
        }
    }
}
