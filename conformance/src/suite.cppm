// okc.suite --- the driver.
//
// It knows how to run a section and in what order. Which sections exist is
// okc.spec's business, and what each one examines is the section's, so adding
// an interface to the specification means adding a section and a row and
// touching neither this module nor the report.
export module okc.suite;

export namespace okc {

// Runs every section, in the order clause 3 lists the interfaces, and returns
// the status the program exits with.
int run_all();

}  // namespace okc
