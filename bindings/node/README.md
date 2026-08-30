# Node.js binding

This wrapper uses Koffi over the stable C ABI. Install the C++ SDK shared library first and set `QUILIBRIUM_SDK_LIB` if it is not on the dynamic-loader path.

```js
const { SDK } = require('@quilibrium/cpp-sdk');
const q = new SDK();
const user = JSON.parse(q.userByFid(3).body.toString('utf8'));
console.log(user.user.username);
q.close();
```
