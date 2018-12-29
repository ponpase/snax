const snaxjs = require("@snaxfoundation/snaxjs");
const fetch = require("node-fetch");
const { promisify } = require("util");
const child_process = require("child_process");
const [exec, execFile, spawn] = [child_process.exec, child_process.execFile]
  .map(promisify)
  .concat(child_process.spawn);
const { TextDecoder, TextEncoder } = require("text-encoding");

const rpc = new snaxjs.JsonRpc(
  process.env.SNAXNODE || "http://127.0.0.1:8888",
  {
    fetch
  }
);

const { account, privateKey } = {
  account: "platform",
  privateKey: "5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3"
};

const signatureProvider = new snaxjs.JsSignatureProvider([
  "5HvtgZn4wf4vNAe3nRb9vjYfLqvasemsSQckVHxmdAeBRbdPURs",
  "5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3",
  "5JD9AGTuTeD5BXZwGQ5AtwBqHK21aHmYnTetHgk1B3pjj7krT8N",
  "5JcWXD3XkpEYbwiVK9Pd3X5bLxLkaUkkJiST3Y9iA4wFrTeyeVL",
  "5JLYkoKuNXGGvUtzjRnP8DqUwt7xny3YGVaDpeqFDdCJKBoBkNC",
  "5JRjkPFeRVGMRLaAa5gkGiC2acf8KT4NkAw1SZ5R7S1gvcCawZh"
]);
const api = new snaxjs.Api({
  rpc,
  signatureProvider,
  textDecoder: new TextDecoder(),
  textEncoder: new TextEncoder()
});

jest.setTimeout(1e6);

const sleep = time => new Promise(resolve => setTimeout(resolve, time));

describe("Platform", async () => {
  beforeEach(async () => {
    spawn("./setup_platform.sh", [], {
      detached: true,
      stdio: "ignore"
    });
    await sleep(6e3);
  });

  const verifyPendingAccounts = async () => {
    expect(
      (await api.rpc.get_table_rows({
        code: account,
        scope: account,
        table: "peaccounts"
      })).rows.map(({ created, ...object }) => object)
    ).toMatchSnapshot();
  };

  const verifyStatesAndAccounts = async () => {
    const [state, accounts, users] = await Promise.all([
      api.rpc.get_table_rows({
        code: account,
        scope: account,
        table: "state"
      }),
      api.rpc.get_table_rows({
        code: account,
        scope: account,
        table: "paccounts"
      }),
      api.rpc.get_table_rows({
        code: account,
        scope: account,
        table: "pusers"
      })
    ]);
    expect(state).toMatchSnapshot();
    expect(accounts).toMatchSnapshot();
    expect(users).toMatchSnapshot();
  };

  const verifyTransferTable = async () => {
    expect(
      await api.rpc.get_table_rows({
        code: account,
        scope: account,
        table: "transfers"
      })
    ).toMatchSnapshot();
  };

  const verifyAccountsBalances = async accounts => {
    const tables = await Promise.all(
      accounts.map(account =>
        api.rpc.get_table_rows({
          code: "snax.token",
          scope: account,
          table: "accounts"
        })
      )
    );
    expect(tables).toMatchSnapshot();
  };

  const tryCatchExpect = async action => {
    try {
      await action();
      expect(false).toBe(true);
    } catch (e) {
      expect(e.message).toMatchSnapshot();
    }
  };

  const initialize = async () => {
    await api.transact(
      {
        actions: [
          {
            account,
            name: "initialize",
            authorization: [
              {
                actor: account,
                permission: "owner"
              }
            ],
            data: {
              name: "test_platform",
              token_dealer: "snax",
              token_symbol_str: "SNAX",
              precision: 4,
              airdrop: ""
            }
          }
        ]
      },
      {
        blocksBehind: 1,
        expireSeconds: 30
      }
    );
  };

  const addUser = accountObj =>
    api.transact(
      {
        actions: [
          {
            account: account,
            name: "addaccount",
            authorization: [
              {
                actor: account,
                permission: "active"
              }
            ],
            data: {
              ...accountObj
            }
          }
        ]
      },
      {
        blocksBehind: 1,
        expireSeconds: 30
      }
    );

  const socialTransfer = (from, to, amount) =>
    api.transact(
      {
        actions: [
          {
            account: account,
            name: "transfertou",
            authorization: [
              {
                actor: from,
                permission: "active"
              }
            ],
            data: {
              from,
              to,
              amount
            }
          }
        ]
      },
      {
        blocksBehind: 1,
        expireSeconds: 30
      }
    );

  const lockUpdate = () =>
    api.transact(
      {
        actions: [
          {
            account: account,
            name: "lockupdate",
            authorization: [
              {
                actor: account,
                permission: "active"
              }
            ],
            data: {}
          }
        ]
      },
      {
        blocksBehind: 1,
        expireSeconds: 30
      }
    );

  const updatePlatform = async () => {
    await lockUpdate();
    await verifyStatesAndAccounts();
    await api.transact(
      {
        actions: [
          {
            account,
            name: "nextround",
            authorization: [
              {
                actor: account,
                permission: "owner"
              }
            ],
            data: {}
          }
        ]
      },
      {
        blocksBehind: 1,
        expireSeconds: 30
      }
    );
    await verifyAccountsBalances(["test2", "test1", "snax", "platform"]);
    await verifyStatesAndAccounts();
    await api.transact(
      {
        actions: [
          {
            account,
            name: "sendpayments",
            authorization: [
              {
                actor: account,
                permission: "active"
              }
            ],
            data: {
              lower_account_name: "",
              account_count: 1000
            }
          }
        ]
      },
      {
        blocksBehind: 1,
        expireSeconds: 30
      }
    );
  };

  const updateQualityRate = accountObj =>
    api.transact(
      {
        actions: [
          {
            account: account,
            name: "updatear",
            authorization: [
              {
                actor: account,
                permission: "active"
              }
            ],
            data: {
              ...accountObj,
              add_account_if_not_exist: false
            }
          }
        ]
      },
      {
        blocksBehind: 1,
        expireSeconds: 30
      }
    );

  const addPendingAccount = (name, id) =>
    api.transact(
      {
        actions: [
          {
            account: account,
            name: "addpenacc",
            authorization: [
              {
                actor: account,
                permission: "active"
              }
            ],
            data: {
              account: name,
              id
            }
          }
        ]
      },
      {
        blocksBehind: 1,
        expireSeconds: 30
      }
    );

  const dropPendingAccount = name =>
    api.transact(
      {
        actions: [
          {
            account: account,
            name: "droppenacc",
            authorization: [
              {
                actor: account,
                permission: "active"
              }
            ],
            data: {
              account: name
            }
          }
        ]
      },
      {
        blocksBehind: 1,
        expireSeconds: 30
      }
    );

  const updateQualityRateMulti = updates =>
    api.transact(
      {
        actions: [
          {
            account: account,
            name: "updatearmult",
            authorization: [
              {
                actor: account,
                permission: "active"
              }
            ],
            data: { updates, add_account_if_not_exist: false }
          }
        ]
      },
      {
        blocksBehind: 1,
        expireSeconds: 30
      }
    );

  it("should process next round correctly", async () => {
    await initialize();
    await addUser({
      account: "test1",
      id: 123,
      attention_rate: 15.0,
      attention_rate_rating_position: 3
    });
    await addUser({
      account: "test2",
      id: 1105,
      attention_rate: 225.0,
      attention_rate_rating_position: 2
    });
    await addUser({
      account: "test2",
      id: 1200,
      attention_rate: 206.0,
      attention_rate_rating_position: 1
    });
    await addUser({
      account: "test1",
      id: 1007,
      attention_rate: 206.0,
      attention_rate_rating_position: 1
    });
    await updatePlatform();
    await verifyStatesAndAccounts();
    await verifyAccountsBalances(["test2", "test1", "snax", "platform"]);
  });

  it("should work with pending account correctly", async () => {
    await initialize();
    await verifyPendingAccounts();
    await addPendingAccount("test1", 42);
    await verifyPendingAccounts();
    await verifyStatesAndAccounts();
    await addUser({
      account: "test1",
      id: 42,
      attention_rate: 225.0,
      attention_rate_rating_position: 1
    });
    await verifyPendingAccounts();
    await verifyStatesAndAccounts();
  });

  it("should drop pending account correctly", async () => {
    await initialize();
    await verifyPendingAccounts();
    await addPendingAccount("test1", 40);
    await verifyPendingAccounts();
    await dropPendingAccount("test1");
    await verifyStatesAndAccounts();
  });

  it("should process social transfer correctly", async () => {
    await initialize();
    await socialTransfer("test.transf", 15, "20.0000 SNAX");
    await verifyTransferTable();
    await verifyAccountsBalances(["test.transf", "test1"]);
    await addUser({
      account: "test1",
      id: 15,
      attention_rate: 15.0,
      attention_rate_rating_position: 1
    });
    await verifyTransferTable();
    await verifyAccountsBalances(["test.transf", "test1"]);
  });

  it("should initialize correctly", async () => {
    await initialize();
    await verifyStatesAndAccounts();
  });

  it("shouldn't be able to initialize second time", async () => {
    await initialize();
    await tryCatchExpect(initialize);
    await verifyStatesAndAccounts();
  });

  it("should add account correctly", async () => {
    await initialize();
    const result = await addUser({
      account: "test2",
      id: 123,
      attention_rate: 15.0,
      attention_rate_rating_position: 1
    });
    await verifyStatesAndAccounts();
  });

  it("shouldn't be able to add user with the same id second time", async () => {
    await initialize();
    await addUser({
      account: "test2",
      id: 123,
      attention_rate: 15.0,
      attention_rate_rating_position: 1
    });
    await tryCatchExpect(() =>
      addUser({
        account: "test2",
        id: 123,
        attention_rate: 26.0,
        attention_rate_rating_position: 2
      })
    );
    await verifyStatesAndAccounts();
  });

  it("should update account's attention rate correctly", async () => {
    await initialize();
    await addUser({
      account: "test2",
      id: 123,
      attention_rate: 15.0,
      attention_rate_rating_position: 2
    });
    await updateQualityRate({
      id: 123,
      attention_rate: 20.0,
      attention_rate_rating_position: 1
    });
    await verifyStatesAndAccounts();
  });

  it("should update multiple account's attention rate correctly", async () => {
    await initialize();
    await addUser({
      account: "test2",
      id: 123,
      attention_rate: 15.0,
      attention_rate_rating_position: 1
    });
    await addUser({
      account: "test1",
      id: 243,
      attention_rate: 8.0,
      attention_rate_rating_position: 2
    });
    await updateQualityRateMulti([
      { id: 243, attention_rate: 20.0, attention_rate_rating_position: 2 },
      { id: 123, attention_rate: 25.0, attention_rate_rating_position: 1 }
    ]);
    await verifyStatesAndAccounts();
  });

  it("shouldn't be able to update non-existent account's attention rate", async () => {
    await initialize();
    await addUser({
      account: "test2",
      id: 123,
      attention_rate: 15.0,
      attention_rate_rating_position: 1
    });
    await tryCatchExpect(() =>
      updateQualityRate({
        id: 250,
        attention_rate: 20.0,
        attention_rate_rating_position: 2
      })
    );

    await verifyStatesAndAccounts();
  });

  it("shouldn't be able to add account or update attention rate when platform is updating", async () => {
    await initialize();
    await addUser({
      account: "test1",
      id: 1105,
      attention_rate: 225.0,
      attention_rate_rating_position: 1
    });
    await lockUpdate();
    await tryCatchExpect(() =>
      updateQualityRate({
        id: 1105,
        attention_rate: 20.0,
        attention_rate_rating_position: 1
      })
    );
    await tryCatchExpect(() =>
      updateQualityRateMulti([
        {
          id: 1105,
          attention_rate: 20.0,
          attention_rate_rating_position: 1
        }
      ])
    );
    await tryCatchExpect(() =>
      addUser({
        account: "test2",
        id: 123,
        attention_rate: 15.0,
        attention_rate_rating_position: 2
      })
    );
    await verifyStatesAndAccounts();
  });
});
