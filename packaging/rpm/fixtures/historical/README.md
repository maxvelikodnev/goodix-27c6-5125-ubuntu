# Historical software identity fixtures

These are public source inventories, not binaries, biometric templates or pairing
material. The SHA-256 of the complete inventory text is the historical builder's
`source_id`. The installed receipt must pass the canonical ownership, complete
inventory and file-hash checks before the Manager uses an identity; historical
recognition additionally checks the provenance and inventory digest.

| Version / matcher | Immutable Git commit | Source identity |
| --- | --- | --- |
| v2.0.0 / SIGFM | `265c8bb703f4c07c865158a7fed2b397a399ff5e` | `5495a408358ecde77c882bc6ada1dc4018077e44375524ed2a46730ab61272cd` |
| v2.1.0 / SIGFM | `ad48120a1cc547c1e5369df7a7839d45d4a618da` | `3dd3fbaa7e5f5cc2599476110b4b3bc1506e3c24efcb76b2a724857544d8cc46` |
| v2.1.1 / SIGFM | `f367c978c857fd2d7d966237add419123ad66ad3` | `929983b0adf8b07e4b68e872a1de1b99193ee534a1960bf20d880af4a9a06760` |
| v2.2.1 / SIGFM | `60b74af2eb87bf3de8fd151586c6e2fecf4d2352` | `e67de418620282d54de2ecbd56eb7c74ffd3deec27176cad629e0c55a67e9e18` |
| Pre-RPM ChicagoHS 3.0.0 | `c760413c43c87c01e464ed0674c8641cde034d9e` | `bdc3729d6467636d06045190f08e3eb3852dcc84181e080f9182984c3f0cc5a4` |
| RPM ChicagoHS 3.0.0 | `6958af6bac65efa45fb78549b2b78c88dab66c88` | `f259f6f10a60e5facc5a8404f951bce3eb18da2aaefebc8c9a167e4540938d97` |

The Chicago snapshot is the pre-RPM driver lineage qualified as 3.0.0 by the
project owner. No version is inferred from hardware, host caches or GitHub
responses. The only same-version equivalence admitted is the last two identities.
Their inventory difference consists of an added `VERSION` and modified
`production/build-public.py` and `docs/LICENSING_AND_PROVENANCE.md`. The builder
changes add version metadata and an optional build work directory; the licensing
change describes the Manager. Driver, Chicago native, libfprint reference,
material checking and PAM source entries are identical. This is source-level
compatibility evidence, not a claim that independently built binaries have
identical hashes. A later unfamiliar 3.0.0 identity remains blocked, including
when the local payload advances to 3.0.1. Versioned later releases do not need a
new historical registry entry for every update.

`v2.2.1.sha256` and `c760413.sha256` are the exact source inventory texts used by
the representative transaction fixtures. Runtime libraries and protected data in
those tests are explicitly synthetic. Initial historical receipts model the
schema-2 inventories emitted by the referenced builders (SIGFM omitted matcher
and version; Chicago supplied matcher but omitted version). They are not hashes
of a real installed historical binary. Schema-1 fixed inventories remain normally
removable, but no unproven commit is promoted to a released version. Their guard
uses `git:<source_commit/build_commit>`, never the absent-runtime `None` identity.

To reproduce any table entry in a repository containing the immutable commit,
run the following from its root, replacing `commit` with that full hash. This
extracts public source into a temporary directory and computes provenance; it
does not build, install, access a device or inspect installed protected data.

```python
import hashlib, importlib.util, io, subprocess, tarfile, tempfile
from pathlib import Path

commit = 'c760413c43c87c01e464ed0674c8641cde034d9e'
paths = ['production', 'deployment', 'reference', 'libfprint-driver',
         'LICENSES', 'docs', 'Rockytkg']
if subprocess.run(['git', 'cat-file', '-e', commit + ':VERSION'],
                  stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode == 0:
    paths.append('VERSION')
archive = subprocess.check_output(['git', 'archive', commit, *paths])
with tempfile.TemporaryDirectory(prefix='goodix-public-provenance-') as temporary:
    with tarfile.open(fileobj=io.BytesIO(archive)) as source:
        source.extractall(temporary, filter='data')
    spec = importlib.util.spec_from_file_location(
        'historical_builder', Path(temporary) / 'production/build-public.py')
    builder = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(builder)
    inventory = builder.source_inventory().encode()
    print(hashlib.sha256(inventory).hexdigest())
```

`python3 -B packaging/rpm/test_compat_transactions.py` exercises the canonical
migration, rollback, removal and future local update using only temporary paths.
This provides synthetic software evidence; real Discover/PolicyKit, Fedora login,
SELinux enforcement and preservation of populated real state require their
separate VM qualification.
