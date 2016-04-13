# Minimum requirements for TinCan Packages

## About *tincan.xml*
- The TinCan Package must contain the file *tincan.xml*.
- This file must follow the guidelines written [here](https://github.com/RusticiSoftware/launch/blob/master/lms_lrs.md).
- This file must contain, at least, an Activity with an Activity ID and the launch file.

So, the file should be, at least, like this one :
```xml
<?xml version="1.0" encoding="utf-8" ?>
<tincan xmlns="http://projecttincan.com/tincan.xsd">
    <activities>
        <activity id="http://example.com/my-activity-id">
            <launch>index.html</launch>
        </activity>
    </activities>
</tincan>
```

## About statement
- The package should send a statement to the LRS containing the final score.
- This statement must use the verb *http://adlnet.gov/expapi/verbs/passed* or *http://adlnet.gov/expapi/verbs/failed*.
- The score property in this statement should have, at least, the *scaled* property or the *raw* and *max* properties or the *success* property.
- This statement must use the *Activity ID* declared in the *tincan.xml* file.
- The statement must use the *registration UUID* given in parameter of the launch file.

So, the statement should look, at least, like this:
```json
{
    "actor": {
        "objectType": "Agent",
        "mbox_sha1sum": "2e13b73bf295005d5503223fa8ba56eb52707301",
        "name": "admin"
    },
    "verb": {
        "id": "http://adlnet.gov/expapi/verbs/failed",
        "display": {
            "en-US": "failed"
        }
    },
    "object": {
        "objectType": "Activity",
        "id": "http://id.tincanapi.com/activity/tincan-prototypes/golf-example"
    },
    "result": {
        "score": {
            "scaled": 0.33
        }
    },
    "context": {
        "registration": "21ee665f-7111-4324-b92c-d31ebf02b0f4"
    }
}
```