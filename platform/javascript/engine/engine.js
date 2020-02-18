var Engine;
(function() {

	var unloadAfterInit = true;
	var canvas = null;
	var resizeCanvasOnStart = false;
	var customLocale = 'en_US';
	var wasmExt = '.wasm';

	var loader = new Loader();
	var rtenv = null;

	var executableName = '';
	var loadPath = '';
	var loadPromise = null;
	var initPromise = null;
	var stderr = null;
	var stdout = null;
	var progressFunc = null;

	function load(basePath) {
		if (loadPromise == null) {
			loadPath = basePath;
			loadPromise = loader.load(loadPath, wasmExt);
			Preloader.setProgressFunc(progressFunc);
			requestAnimationFrame(Preloader.animateProgress);
		}
		return loadPromise;
	};

	function unload() {
		loadPromise = null;
	};

	Engine = function Engine() { // The Engine class
		this.init = function(basePath) {
			if (initPromise) {
				return initPromise;
			}
			if (!loadPromise) {
				if (!basePath) {
					initPromise = Promise.reject(new Error("A base path must be provided when calling `init` and the engine is not loaded."));
					return initPromise;
				}
				load(basePath);
			}
			var config = {}
			if (typeof stdout === 'function')
				config.print = stdout;
			if (typeof stderr === 'function')
				config.printErr = stderr;
			initPromise = loader.init(loadPromise, loadPath, config).then(function() {
				return new Promise(function(resolve, reject) {
					rtenv = loader.env;
					if (unloadAfterInit) {
						loadPromise = null;
					}
					resolve();
				});
			});
			return initPromise;
		};

		this.preloadFile = function(file, path) {
			return Preloader.preload(file, path);
		};

		this.start = function() {
			// Start from arguments.
			var args = [];
			for (var i = 0; i < arguments.length; i++) {
				args.push(arguments[i]);
			}
			var me = this;
			return new Promise(function(resolve, reject) {
				return me.init().then(function() {
					if (!(canvas instanceof HTMLCanvasElement)) {
						canvas = Utils.findCanvas();
					}
					rtenv['locale'] = customLocale;
					rtenv['canvas'] = canvas;
					rtenv['thisProgram'] = executableName;
					rtenv['resizeCanvasOnStart'] = resizeCanvasOnStart;
					loader.start(args).then(function() {
						loader = null;
						initPromise = null;
						resolve();
					});
				});
			});
		};

		this.startGame = function(execName, mainPack) {
			// Start and init with execName as loadPath if not inited.
			executableName = execName;
			var me = this;
			return Promise.all([
				this.init(execName),
				this.preloadFile(mainPack, mainPack)
			]).then(function() {
				return me.start('--main-pack', mainPack);
			});
		};

		this.setWebAssemblyFilenameExtension = function(override) {
			if (String(override).length === 0) {
				throw new Error('Invalid WebAssembly filename extension override');
			}
			wasmExt = String(override);
		};

		this.setUnloadAfterInit = function(enabled) {
			unloadAfterInit = enabled;
		};

		this.setCanvas = function(canvasElem) {
			canvas = canvasElem;
		};

		this.setCanvasResizedOnStart = function(enabled) {
			resizeCanvasOnStart = enabled;
		};

		this.setLocale = function(locale) {
			customLocale = locale;
		};

		this.setExecutableName = function(newName) {
			executableName = newName;
		};

		this.setProgressFunc = function(func) {
			progressFunc = func;
		}

		this.setStdoutFunc = function(func) {

			var print = function(text) {
				if (arguments.length > 1) {
					text = Array.prototype.slice.call(arguments).join(" ");
				}
				func(text);
			};
			if (rtenv)
				rtenv.print = print;
			stdout = print;
		};

		this.setStderrFunc = function(func) {

			var printErr = function(text) {
				if (arguments.length > 1)
					text = Array.prototype.slice.call(arguments).join(" ");
				func(text);
			};
			if (rtenv)
				rtenv.printErr = printErr;
			stderr = printErr;
		};
	}; // Engine class end.

	Engine.isWebGLAvailable = Utils.isWebGLAvailable;
	Engine.load = load;
	Engine.unload = unload;
})();
