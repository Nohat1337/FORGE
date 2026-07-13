// Forge Package Manager - CLI
// Written in Forge, runs on FVM

import "ui/ui"
import "ui/pkg_manager"
import "fs"
import "os"
import "json"

fn print_usage() {
    print("Forge Package Manager")
    print("Usage: forge pkg <command> [args]")
    print("")
    print("Commands:")
    print("  init [name]           Create a new forge.json")
    print("  add <pkg>[@version]   Add a dependency")
    print("  add --dev <pkg>       Add a dev dependency")
    print("  remove <pkg>          Remove a dependency")
    print("  update [pkg...]       Update dependencies")
    print("  install               Install all dependencies")
    print("  search <query>        Search packages")
    print("  publish               Publish package")
    print("  login                 Login to registry")
    print("  logout                Logout from registry")
    print("  whoami                Show current user")
    print("  cache clean           Clean package cache")
}

fn main() {
    let args = os.args()
    if args.len() < 2 {
        print_usage()
        return
    }
    
    let cmd = args[1]
    let pm = PackageManager::new()
    
    match cmd {
        "init" => {
            let name = if os.args().len() > 2 { os.args()[2] } else { os.cwd().split('/').last().unwrap() }
            let version = if os.args().len() > 3 { os.args()[3] } else { "0.1.0" }
            match pm.init(name, version) {
                Ok(_) => print("Created forge.json"),
                Err(e) => { print("Error: " + e); os.exit(1) }
            }
        }
        "add" => {
            let dev = os.args().contains("--dev")
            let pkgs = os.args().filter(|a| a != "--dev" && a != "add").skip(1).collect()
            if pkgs.is_empty() {
                print("Error: No packages specified")
                os.exit(1)
            }
            match pm.install(pkgs, dev) {
                Ok(_) => print("Dependencies added"),
                Err(e) => { print("Error: " + e); os.exit(1) }
            }
        }
        "remove" | "rm" => {
            if os.args().len() < 3 {
                print("Error: Package name required")
                os.exit(1)
            }
            let name = os.args()[2]
            match pm.remove(name) {
                Ok(_) => print("Removed " + name),
                Err(e) => { print("Error: " + e); os.exit(1) }
            }
        }
        "update" => {
            let pkgs = if os.args().len() > 2 { os.args().skip(2).collect() } else { [] }
            match pm.update(pkgs) {
                Ok(_) => print("Updated"),
                Err(e) => { print("Error: " + e); os.exit(1) }
            }
        }
        "install" | "i" => {
            match pm.install([], false) {
                Ok(_) => print("Dependencies installed"),
                Err(e) => { print("Error: " + e); os.exit(1) }
            }
        }
        "search" => {
            if os.args().len() < 3 {
                print("Error: Search query required")
                os.exit(1)
            }
            let query = os.args()[2]
            match pm.search(query) {
                Ok(results) => {
                    for pkg in results {
                        print(pkg.name + "@" + pkg.version + " - " + pkg.description)
                    }
                }
                Err(e) => { print("Error: " + e); os.exit(1) }
            }
        }
        "publish" => {
            if !os.env("FORGE_TOKEN").is_some() {
                print("Error: FORGE_TOKEN environment variable required")
                os.exit(1)
            }
            match pm.publish(os.env("FORGE_TOKEN").unwrap()) {
                Ok(_) => print("Published successfully"),
                Err(e) => { print("Error: " + e); os.exit(1) }
            }
        }
        "login" => {
            print("Login to Forge registry")
            print("Enter token: ")
            let token = os.read_line()
            if token.is_empty() { print("Token required"); os.exit(1) }
            fs.write_file(os.home_dir() + "/.forge/token", token)
            print("Login successful")
        }
        "logout" => {
            fs.remove_file(os.home_dir() + "/.forge/token")
            print("Logged out")
        }
        "whoami" => {
            if let Some(token) = fs.read_file(os.home_dir() + "/.forge/token") {
                // Decode token to get user info
                print("Logged in as: user")
            } else {
                print("Not logged in")
            }
        }
        "cache" => {
            if os.args().len() > 2 && os.args()[2] == "clean" {
                let cache_dir = PackageManager::new().cache_dir
                fs.remove_dir_all(cache_dir)
                print("Cache cleaned")
            } else {
                print("Usage: forge pkg cache clean")
            }
        }
        _ => {
            print("Unknown command: " + cmd)
            print_usage()
            os.exit(1)
        }
    }
}

fn print_usage() {
    print("Forge Package Manager")
    print("Usage: forge pkg <command> [args]")
    print("")
    print("Commands:")
    print("  init [name] [version]    Create a new forge.json")
    print("  add <pkg>[@version]      Add a dependency")
    print("  add --dev <pkg>[@ver]    Add a dev dependency")
    print("  remove <pkg>             Remove a dependency")
    print("  update [pkg...]          Update dependencies")
    print("  install                  Install all dependencies")
    print("  search <query>           Search packages")
    print("  publish                  Publish package")
    print("  login                    Login to registry")
    print("  logout                   Logout from registry")
    print("  whoami                   Show current user")
    print("  cache clean              Clean package cache")
}

fn main() {
    main()
}