var Loader = function() {

	this.env = null;

	this.load = function(basePath, wasmExt) {
		return Preloader.loadPromise(basePath + wasmExt);
	}

	this.init = function(loadPromise, basePath, config) {
		var me = this;
		return new Promise(function(resolve, reject) {
			var cfg = config || {};
			cfg['locateFile'] = Utils.createLocateRewrite(basePath);
			cfg['instantiateWasm'] = Utils.createInstantiatePromise(loadPromise);
			loadPromise = null;
			Godot(cfg).then(function(module) {
				me.env = module;
				resolve();
			});
		});
	}

	this.start = function(args) {
		var me = this;
		return new Promise(function(resolve, reject) {
			if (!me.env) {
				reject(new Error('The engine must be initialized before it can be started'));
			}
			Preloader.preloadedFiles.forEach(function(file) {
				Utils.copyToFS(me.env['FS'], file.path, file.buffer);
			});
			Preloader.preloadedFiles = [];
			me.env['callMain'](args);
			resolve();
		});
	}
};
