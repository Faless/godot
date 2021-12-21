// This service worker is required to expose an exported Godot project as a
// Progressive Web App. It provides an offline fallback page telling the user
// that they need an Internet connection to run the project if desired.
// Incrementing CACHE_VERSION will kick off the install event and force
// previously cached resources to be updated from the network.
const CACHE_VERSION = "@GODOT_VERSION@";
const CACHE_NAME = "@GODOT_NAME@-cache";
const OFFLINE_URL = "@GODOT_OFFLINE_PAGE@";
// Files that will be cached on load.
const CACHED_FILES = @GODOT_CACHE@;
// Files that we might not want the user to preload, and will only be cached on first load.
const CACHABLE_FILES = @GODOT_OPT_CACHE@;
const FULL_CACHE = CACHED_FILES.concat(CACHABLE_FILES);

self.addEventListener("install", (event) => {
	event.waitUntil(async function () {
		const cache = await caches.open(CACHE_NAME);
		// Clear old cache (including optionals).
		await Promise.all(FULL_CACHE.map(path => cache.delete(path)));
		// Insert new one.
		const done = await cache.addAll(CACHED_FILES);
		return done;
	}());
});

self.addEventListener("activate", (event) => {
	event.waitUntil(async function () {
		if ("navigationPreload" in self.registration) {
			await self.registration.navigationPreload.enable();
		}
	}());
	// Tell the active service worker to take control of the page immediately.
	self.clients.claim();
});

self.addEventListener("fetch", (event) => {
	const isNavigate = event.request.mode === "navigate";
	const url = event.request.url || "";
	const referrer = event.request.referrer || "";
	const base = referrer.slice(0, referrer.lastIndexOf("/") + 1);
	const local = url.startsWith(base) ? url.replace(base, "") : "";
	const isCachable = FULL_CACHE.some(v => v === local) || (base === referrer && base.endsWith(CACHED_FILES[0]));
	if (isNavigate || isCachable) {
		event.respondWith(async function () {
			// Try to use cache first
			const cache = await caches.open(CACHE_NAME);
			if (event.request.mode === "navigate") {
				// Check if we have full cache during HTML page request.
				const fullCache = await Promise.all(FULL_CACHE.map(name => cache.match(name)));
				const missing = fullCache.some(v => v === undefined);
				if (missing) {
					try {
						// Always go over network is some cached file is missing.
						const response = await fetch(event.request);
						return response;
					} catch (e) {
						// And return the hopefully always cached offline page in case of network failure.
						return await caches.match(OFFLINE_URL);
					}
				}
			}
			const cached = await cache.match(event.request);
			if (cached) {
				return cached;
			} else {
				// Use the preloaded response, if it's there
				let request = event.request.clone();
				let response = await event.preloadResponse;
				if (!response) {
					// Or, go over network.
					response = await fetch(event.request);
				}
				if (isCachable) {
					// And update the cache
					cache.put(request, response.clone());
				}
				return reponse;
			}
		}());
	}
});
