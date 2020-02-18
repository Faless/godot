var Preloader = (function() {

	var DOWNLOAD_ATTEMPTS_MAX = 4;

	function loadXHR(resolve, reject, file, tracker) {
		var xhr = new XMLHttpRequest;
		xhr.open('GET', file);
		if (!file.endsWith('.js')) {
			xhr.responseType = 'arraybuffer';
		}
		['loadstart', 'progress', 'load', 'error', 'abort'].forEach(function(ev) {
			xhr.addEventListener(ev, onXHREvent.bind(xhr, resolve, reject, file, tracker));
		});
		xhr.send();
	}

	function onXHREvent(resolve, reject, file, tracker, ev) {

		if (this.status >= 400) {

			if (this.status < 500 || ++tracker[file].attempts >= DOWNLOAD_ATTEMPTS_MAX) {
				reject(new Error("Failed loading file '" + file + "': " + this.statusText));
				this.abort();
				return;
			} else {
				setTimeout(loadXHR.bind(null, resolve, reject, file, tracker), 1000);
			}
		}

		switch (ev.type) {
			case 'loadstart':
				if (tracker[file] === undefined) {
					tracker[file] = {
						total: ev.total,
						loaded: ev.loaded,
						attempts: 0,
						final: false,
					};
				}
				break;

			case 'progress':
				tracker[file].loaded = ev.loaded;
				tracker[file].total = ev.total;
				break;

			case 'load':
				tracker[file].final = true;
				resolve(this);
				break;

			case 'error':
				if (++tracker[file].attempts >= DOWNLOAD_ATTEMPTS_MAX) {
					tracker[file].final = true;
					reject(new Error("Failed loading file '" + file + "'"));
				} else {
					setTimeout(loadXHR.bind(null, resolve, reject, file, tracker), 1000);
				}
				break;

			case 'abort':
				tracker[file].final = true;
				reject(new Error("Loading file '" + file + "' was aborted."));
				break;
		}
	}

	var loadingFiles = {};
	function loadPromise(file) {
		return new Promise(function(resolve, reject) {
			loadXHR(resolve, reject, file, loadingFiles);
		});
	}

	var preloadedFiles = [];
	function preloadFile(pathOrBuffer, destPath) {
		if (pathOrBuffer instanceof ArrayBuffer) {
			pathOrBuffer = new Uint8Array(pathOrBuffer);
		} else if (ArrayBuffer.isView(pathOrBuffer)) {
			pathOrBuffer = new Uint8Array(pathOrBuffer.buffer);
		}
		if (pathOrBuffer instanceof Uint8Array) {
			preloadedFiles.push({
				path: destPath,
				buffer: pathOrBuffer
			});
			return Promise.resolve();
		} else if (typeof pathOrBuffer === 'string') {
			return loadPromise(pathOrBuffer, loadingFiles).then(function(xhr) {
				preloadedFiles.push({
					path: destPath || pathOrBuffer,
					buffer: xhr.response
				});
				return Promise.resolve();
			});
		} else {
			throw Promise.reject("Invalid object for preloading");
		}
	};

	return {
		'preload': preloadFile,
		'loadPromise': loadPromise,
		'loadingTracker': loadingFiles,
		'preloadedFiles': preloadedFiles
	}
})();
