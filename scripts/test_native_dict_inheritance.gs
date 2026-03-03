import system as system;

class CountingDict extends Dict {
    fn __new__(self) {
        self.writeCount = 0;
    }

    fn set(self, key, value) {
        self.writeCount = self.writeCount + 1;
        return super.set(self, key, value);
    }

    fn get(self, key) {
        let value = super.get(self, key);
        if (value == null) {
            return -1;
        }
        return value;
    }

    fn size(self) {
        return super.size(self);
    }
}

fn benchmark_once() {
    let d = CountingDict();
    for (i in range(0, 1000)) {
        d[i] = i + 3;
    }

    let checksum = 0;
    for (i in range(0, 1000)) {
        checksum = checksum + d[i];
    }

    assert(d.writeCount == 1000, "writeCount expected 1000, actual {}", d.writeCount);
    assert(d.size() == 1000, "size expected 1000, actual {}", d.size());
    assert(checksum == 502500, "checksum expected 502500, actual {}", checksum);
    return checksum;
}

fn main() {
    print("[test] native dict inheritance start");

    let d = CountingDict();
    d[1] = 11;
    d[2] = 22;
    assert(d[1] == 11, "d[1] expected 11, actual {}", d[1]);
    assert(d[3] == -1, "missing key expected -1, actual {}", d[3]);
    assert(d.writeCount == 2, "writeCount expected 2, actual {}", d.writeCount);
    assert(1 in d, "key 1 should be in d");
    assert(9 not in d, "key 9 should not be in d");

    let begin = system.getTimeMs();
    let total = 0;
    for (i in range(0, 5)) {
        total = total + benchmark_once();
    }
    let end = system.getTimeMs();

    print("[test] benchmark checksum", total);
    print("[test] benchmark elapsed(ms)", end - begin);
    assert(total == 2512500, "benchmark total expected 2512500, actual {}", total);

    print("[test] native dict inheritance passed");
    return 0;
}
