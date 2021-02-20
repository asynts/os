use std::error::Error;

fn main() -> Result<(), Box<dyn Error>> {
    cc::Build::new().file("src/init.s").compile("init");

    println!("cargo:rerun-if-changed=src/init.s");
    println!("cargo:rerun-if-changed=link.x");

    Ok(())
}
