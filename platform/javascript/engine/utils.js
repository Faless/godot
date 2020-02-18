var Utils = (function() {

	function createLocateRewrite(execName) {
		function rw(path) {
			if (path.endsWith('.worker.js')) {
				return execName + '.worker.js';
			} else if (path.endsWith('.js')) {
				return execName + '.js';
			} else if (path.endsWith('.wasm')) {
				return execName + '.wasm';
			}
		}
		return rw;
	}

	function createInstantiatePromise(wasmLoader) {
		function instantiateWasm(imports, onSuccess) {
			wasmLoader.then(function(xhr) {
				WebAssembly.instantiate(xhr.response, imports).then(function(result) {
					onSuccess(result.instance, result.module);
				});
			});
			wasmLoader = null;
			return {};
		};

		return instantiateWasm;
	}

	function copyToFS(fs, path, buffer) {
		var p = path.lastIndexOf("/");
		var dir = "/";
		if (p > 0) {
			dir = path.slice(0, file.path.lastIndexOf("/"));
		}
		try {
			fs.stat(dir);
		} catch (e) {
			if (e.errno !== 44) { // 'ENOENT', see https://github.com/emscripten-core/emscripten/blob/master/system/lib/libc/musl/arch/emscripten/bits/errno.h
				throw e;
			}
			fs.mkdirTree(dir);
		}
		// With memory growth, canOwn should be false.
		fs.writeFile(path, new Uint8Array(buffer), {'flags': 'wx+'});
	}

	function findCanvas() {
		var nodes = document.getElementsByTagName('canvas');
		if (nodes.length && nodes[0] instanceof HTMLCanvasElement) {
			return nodes[0];
		}
		throw new Error("No canvas found");
	}

	function isWebGLAvailable(majorVersion = 1) {

		var testContext = false;
		try {
			var testCanvas = document.createElement('canvas');
			if (majorVersion === 1) {
				testContext = testCanvas.getContext('webgl') || testCanvas.getContext('experimental-webgl');
			} else if (majorVersion === 2) {
				testContext = testCanvas.getContext('webgl2') || testCanvas.getContext('experimental-webgl2');
			}
		} catch (e) {}
		return !!testContext;
	}

	return {
		'createLocateRewrite': createLocateRewrite,
		'createInstantiatePromise': createInstantiatePromise,
		'copyToFS': copyToFS,
		'findCanvas': findCanvas,
		'isWebGLAvailable': isWebGLAvailable
	};
})();
