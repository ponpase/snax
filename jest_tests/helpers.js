module.exports = {
  tryCatchExpect: async action => {
    try {
      await action();
      throw {
        get message() {
          throw new Error("Unexpected!");
        }
      };
    } catch (e) {
      expect(e.message).toMatchSnapshot();
    }
  },
  sleep: time => new Promise(resolve => setTimeout(resolve, time)),
  pick: (keys, obj) =>
    keys.reduce((acc, key) => ({ ...acc, [key]: obj[key] }), {})
};
