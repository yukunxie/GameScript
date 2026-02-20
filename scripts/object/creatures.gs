fn animalVoice() {
    print("animal");
    return 1;
}

fn dogVoice() {
    print("dog");
    return 2;
}

class Animal {
    Name = "Cat";
    VoiceFn = animalVoice;

    fn __new__(self) {
        return 0;
    }

    fn __delete__(self) {
        print("__delete__ Animal", self.Name);
        return 0;
    }

    fn speak(self) {
        print(self.Name);
        return self.VoiceFn();
    }
}

class Dog extends Animal {
    Name = "Dog";
    VoiceFn = dogVoice;

    fn __new__(self) {
        return 0;
    }

    fn speak(self) {
        print(self.Name);
        return self.VoiceFn();
    }
}
