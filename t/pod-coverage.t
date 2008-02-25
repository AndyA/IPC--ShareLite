#!perl -T

use Test::More;
eval "use Test::Pod::Coverage 1.04";

plan skip_all =>
  "Test::Pod::Coverage 1.04 required for testing POD coverage"
  if $@;

all_pod_coverage_ok(
    {
        private => [
            qr{^DESTROY|AUTOLOAD|bootstrap$},
            qr{^shlock|shunlock$},
            qr{^constant|destroy_share|new_share|read_share|sharelite_.*|write_share$},
            qr{^(?:GET|IPC_|LOCK_|SEM_|SET).*$},
            qr{^_}
        ]
    }
);

