'use strict';

const { Object } = primordials;
const {
  ERR_MANIFEST_DEPENDENCY_MISSING,
  ERR_UNKNOWN_BUILTIN_MODULE
} = require('internal/errors').codes;
const { NativeModule } = require('internal/bootstrap/loaders');
const { getOptionValue } = require('internal/options');
const experimentalModules = getOptionValue('--experimental-modules');

const { validateString } = require('internal/validators');
const path = require('path');
const { pathToFileURL, fileURLToPath } = require('internal/url');
const { URL } = require('url');

const debug = require('internal/util/debuglog').debuglog('module');

function loadNativeModule(filename, request, experimentalModules) {
  const mod = NativeModule.map.get(filename);
  if (mod) {
    debug('load native module %s', request);
    mod.compileForPublicLoader(experimentalModules);
    return mod;
  }
}

// Invoke with makeRequireFunction(module) where |module| is the Module object
// to use as the context for the require() function.
// Use redirects to set up a mapping from a policy and restrict dependencies
function makeRequireFunction(mod, redirects) {
  const Module = mod.constructor;

  let require;
  if (redirects) {
    const { map, reaction } = redirects;
    const id = mod.filename || mod.id;
    require = function require(path) {
      let missing = true;
      if (map === true) {
        missing = false;
      } else if (map.has(path)) {
        const redirect = map.get(path);
        if (redirect === true) {
          missing = false;
        } else if (typeof redirect === 'string') {
          const parsed = new URL(redirect);
          if (parsed.protocol === 'node:') {
            const specifier = parsed.pathname;
            const mod = loadNativeModule(
              specifier,
              redirect,
              experimentalModules);
            if (mod && mod.canBeRequiredByUsers) return mod.exports;
            throw new ERR_UNKNOWN_BUILTIN_MODULE(specifier);
          } else if (parsed.protocol === 'file:') {
            return mod.require(fileURLToPath(parsed));
          }
        }
      }
      if (missing) {
        reaction(new ERR_MANIFEST_DEPENDENCY_MISSING(id, path));
      }
      return mod.require(path);
    };
  } else {
    require = function require(path) {
      return mod.require(path);
    };
  }

  function resolve(request, options) {
    validateString(request, 'request');
    return Module._resolveFilename(request, mod, false, options);
  }

  require.resolve = resolve;

  function paths(request) {
    validateString(request, 'request');
    return Module._resolveLookupPaths(request, mod);
  }

  resolve.paths = paths;

  require.main = process.mainModule;

  // Enable support to add extra extension types.
  require.extensions = Module._extensions;

  require.cache = Module._cache;

  return require;
}

/**
 * Remove byte order marker. This catches EF BB BF (the UTF-8 BOM)
 * because the buffer-to-string conversion in `fs.readFileSync()`
 * translates it to FEFF, the UTF-16 BOM.
 */
function stripBOM(content) {
  if (content.charCodeAt(0) === 0xFEFF) {
    content = content.slice(1);
  }
  return content;
}

const builtinLibs = [
  'assert', 'async_hooks', 'buffer', 'child_process', 'cluster', 'crypto',
  'dgram', 'dns', 'domain', 'events', 'fs', 'http', 'http2', 'https', 'net',
  'os', 'path', 'perf_hooks', 'punycode', 'quic', 'querystring', 'readline',
  'repl', 'stream', 'string_decoder', 'tls', 'trace_events', 'tty', 'url',
  'util', 'v8', 'vm', 'worker_threads', 'zlib'
];

if (typeof internalBinding('inspector').open === 'function') {
  builtinLibs.push('inspector');
  builtinLibs.sort();
}

function addBuiltinLibsToObject(object) {
  // Make built-in modules available directly (loaded lazily).
  builtinLibs.forEach((name) => {
    // Goals of this mechanism are:
    // - Lazy loading of built-in modules
    // - Having all built-in modules available as non-enumerable properties
    // - Allowing the user to re-assign these variables as if there were no
    //   pre-existing globals with the same name.

    const setReal = (val) => {
      // Deleting the property before re-assigning it disables the
      // getter/setter mechanism.
      delete object[name];
      object[name] = val;
    };

    Object.defineProperty(object, name, {
      get: () => {
        const lib = require(name);

        // Disable the current getter/setter and set up a new
        // non-enumerable property.
        delete object[name];
        Object.defineProperty(object, name, {
          get: () => lib,
          set: setReal,
          configurable: true,
          enumerable: false
        });

        return lib;
      },
      set: setReal,
      configurable: true,
      enumerable: false
    });
  });
}

function normalizeReferrerURL(referrer) {
  if (typeof referrer === 'string' && path.isAbsolute(referrer)) {
    return pathToFileURL(referrer).href;
  }
  return new URL(referrer).href;
}

module.exports = {
  addBuiltinLibsToObject,
  builtinLibs,
  loadNativeModule,
  makeRequireFunction,
  normalizeReferrerURL,
  stripBOM,
};
