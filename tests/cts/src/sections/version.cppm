// What the implementation says about itself before it is used.
//
// Not conditional on a feature: `kal_version' and `kal_interfaces' belong to no
// interface and every conforming implementation exports them.
export module okc.version;

export namespace okc::version { void run(); }
