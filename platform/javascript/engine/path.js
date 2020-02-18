
var Path = (function() {
	function dirname(p_path) {

		var path = p_path;
		if (path == '/') {
			return path;
		}
		if (!path.startsWith('/') && !path.startsWith('./')) {
			path = './' + path;
		}
		if (path.endsWith('/')) {
			path = path.slice(0, -1);
		}

		const ls = path.lastIndexOf("/");
		if (ls > 0) {
			path = path.slice(0, ls);
		}
		return path;
	}

	function basename(p_path) {

		var path = p_path
		while (path.endsWith('/'))
			path = path.slice(0, -1);

		const ls = path.lastIndexOf('/');
		if (ls >= 0) {
			path = path.slice(0, ls);
		}
		return path;
	}

	function suffix(p_path) {
		const name = basename(p_path);
		const ls = name.lastIndexOf('.');
		if (ls < 0) {
			return '';
		}
		return name.slice(ls);
	}

	function prefix(p_path) {
		var name = basename(p_path);
		var ls = name.lastIndexOf('.');
		if (ls < 0) {
			return name;
		}
		return name.slice(0, ls);
	}

	return {
		'dirname': dirname,
		'basename': basename,
		'suffix': suffix,
		'prefix': prefix
	}
})();
