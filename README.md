<div align="center">
  
  ### VALP
  
[![Linux](https://github.com/valp-lang/valp/actions/workflows/linux.yml/badge.svg?branch=master)](https://github.com/valp-lang/valp/actions/workflows/linux.yml)
[![MacOS](https://github.com/valp-lang/valp/actions/workflows/macos.yml/badge.svg?branch=master)](https://github.com/valp-lang/valp/actions/workflows/macos.yml)
[![Stars](https://img.shields.io/github/stars/valp-lang/valp?logo=github)](https://github.com/valp-lang/valp)

</div>


### Overview
Valp is a scripting language with stack based VM written in C.

### Installing

```
$ git clone -b master https://github.com/valp-lang/valp
$ cd valp
$ make
$ make repl // to run repl session
```

### Example code

```
print("Hello, world!")

// Everything after '//' is treated as an comment.

class Foo {
	init(bar) {
		self.bar = bar;
	}

	def baz(baz) {
		print(self.bar + baz);
	}
}

var foo = Foo("Hello, ")
f.baz("world!")
```

Check [here](https://github.com/valp-lang/valp/blob/master/docs/examples) for more examples.

### Playground

TODO: complete when create playground 

### Contributing

All contributors are welcome, for more details check [CONTRIBUTING](https://github.com/valp-lang/valp/blob/master/CONTRIBUTING.md).

### VSCode extension

You can install VSCode extension from [here](https://github.com/valp-lang/valp-vscode).

### License

Distributed under the MIT License. See [LICENSE](https://github.com/valp-lang/valp/blob/master/LICENSE) for more information.
